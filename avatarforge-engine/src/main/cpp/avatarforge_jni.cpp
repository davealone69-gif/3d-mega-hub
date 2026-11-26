#include <jni.h>
#include <android/bitmap.h>
#include <dlfcn.h>

#include "afcore.h"
#include "af_glb_read.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>

namespace {
JavaVM* gJavaVm = nullptr;

bool decodeViaAndroidBitmapFactory(const uint8_t* data, std::size_t size,
                                   af::DecodedImage* out, std::string* error) {
    if (!gJavaVm || !data || size == 0 || size > 64u * 1024u * 1024u || !out) {
        if (error) *error = "JNI BitmapFactory fallback is unavailable or received invalid input";
        return false;
    }
    struct BitmapNdkApi {
        void* lib = dlopen("libjnigraphics.so", RTLD_NOW | RTLD_LOCAL);
        using GetInfo = int (*)(JNIEnv*, jobject, AndroidBitmapInfo*);
        using Lock = int (*)(JNIEnv*, jobject, void**);
        using Unlock = int (*)(JNIEnv*, jobject);
        GetInfo getInfo = lib ? reinterpret_cast<GetInfo>(dlsym(lib, "AndroidBitmap_getInfo")) : nullptr;
        Lock lock = lib ? reinterpret_cast<Lock>(dlsym(lib, "AndroidBitmap_lockPixels")) : nullptr;
        Unlock unlock = lib ? reinterpret_cast<Unlock>(dlsym(lib, "AndroidBitmap_unlockPixels")) : nullptr;
        bool ready() const { return getInfo && lock && unlock; }
    };
    static const BitmapNdkApi ndk;
    if (!ndk.ready()) {
        if (error) *error = "libjnigraphics AndroidBitmap API unavailable";
        return false;
    }

    JNIEnv* env = nullptr;
    bool attached = false;
    const jint envStat = gJavaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (envStat == JNI_EDETACHED) {
        if (gJavaVm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
            if (error) *error = "failed to attach worker thread to JVM for BitmapFactory decode";
            return false;
        }
        attached = true;
    } else if (envStat != JNI_OK || !env) {
        if (error) *error = "JVM environment unavailable for BitmapFactory decode";
        return false;
    }
    struct DetachGuard {
        JavaVM* vm;
        bool shouldDetach;
        ~DetachGuard() { if (vm && shouldDetach) vm->DetachCurrentThread(); }
    } detachGuard{gJavaVm, attached};

    if (env->PushLocalFrame(16) < 0) {
        env->ExceptionClear();
        if (error) *error = "JNI PushLocalFrame failed during image decode";
        return false;
    }
    struct FrameGuard {
        JNIEnv* e;
        ~FrameGuard() { e->PopLocalFrame(nullptr); }
    } frameGuard{env};

    jclass factoryCls = env->FindClass("android/graphics/BitmapFactory");
    jclass optsCls = env->FindClass("android/graphics/BitmapFactory$Options");
    jclass configCls = env->FindClass("android/graphics/Bitmap$Config");
    jclass bmpCls = env->FindClass("android/graphics/Bitmap");
    if (!factoryCls || !optsCls || !configCls || !bmpCls) {
        env->ExceptionClear();
        if (error) *error = "Android BitmapFactory classes not found";
        return false;
    }
    jmethodID optsCtor = env->GetMethodID(optsCls, "<init>", "()V");
    jfieldID inPremulField = env->GetFieldID(optsCls, "inPremultiplied", "Z");
    jfieldID inConfigField = env->GetFieldID(optsCls, "inPreferredConfig", "Landroid/graphics/Bitmap$Config;");
    jfieldID argbField = env->GetStaticFieldID(configCls, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
    jmethodID decodeMethod = env->GetStaticMethodID(
        factoryCls, "decodeByteArray", "([BIILandroid/graphics/BitmapFactory$Options;)Landroid/graphics/Bitmap;");
    jmethodID recycleMethod = env->GetMethodID(bmpCls, "recycle", "()V");
    if (!optsCtor || !inPremulField || !inConfigField || !argbField || !decodeMethod || !recycleMethod) {
        env->ExceptionClear();
        if (error) *error = "Android BitmapFactory methods not found";
        return false;
    }

    jobject opts = env->NewObject(optsCls, optsCtor);
    jobject argbConfig = env->GetStaticObjectField(configCls, argbField);
    jbyteArray inputBytes = env->NewByteArray(static_cast<jsize>(size));
    if (!opts || !argbConfig || !inputBytes) {
        env->ExceptionClear();
        if (error) *error = "out of JNI memory allocating BitmapFactory input";
        return false;
    }
    env->SetBooleanField(opts, inPremulField, JNI_FALSE);
    env->SetObjectField(opts, inConfigField, argbConfig);
    env->SetByteArrayRegion(inputBytes, 0, static_cast<jsize>(size), reinterpret_cast<const jbyte*>(data));

    jobject bitmap = env->CallStaticObjectMethod(factoryCls, decodeMethod, inputBytes, 0, static_cast<jint>(size), opts);
    if (env->ExceptionCheck() || !bitmap) {
        env->ExceptionClear();
        if (error) *error = "BitmapFactory.decodeByteArray rejected embedded image";
        return false;
    }

    AndroidBitmapInfo info{};
    bool ok = false;
    if (ndk.getInfo(env, bitmap, &info) == ANDROID_BITMAP_RESULT_SUCCESS &&
        info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 &&
        info.width > 0 && info.height > 0 && info.width <= 8192 && info.height <= 8192 &&
        static_cast<std::size_t>(info.width) * static_cast<std::size_t>(info.height) * 4u <= 64u * 1024u * 1024u) {
        void* pixels = nullptr;
        if (ndk.lock(env, bitmap, &pixels) == ANDROID_BITMAP_RESULT_SUCCESS && pixels) {
            try {
                const std::size_t rowBytes = static_cast<std::size_t>(info.width) * 4u;
                out->width = static_cast<int>(info.width);
                out->height = static_cast<int>(info.height);
                out->rgba.resize(rowBytes * static_cast<std::size_t>(info.height));
                const uint8_t* src = static_cast<const uint8_t*>(pixels);
                for (uint32_t y = 0; y < info.height; ++y) {
                    std::memcpy(out->rgba.data() + static_cast<std::size_t>(y) * rowBytes,
                                src + static_cast<std::size_t>(y) * info.stride, rowBytes);
                }
                ok = true;
            } catch (const std::bad_alloc&) {
                out->width = 0;
                out->height = 0;
                out->rgba.clear();
                if (error) *error = "out of memory copying decoded Bitmap pixels";
            }
            ndk.unlock(env, bitmap);
        } else if (error) {
            *error = "AndroidBitmap_lockPixels failed";
        }
    } else if (error) {
        *error = "decoded Bitmap dimensions or format outside supported RGBA8888 1..8192 limits";
    }
    env->CallVoidMethod(bitmap, recycleMethod);
    env->ExceptionClear();
    return ok;
}
constexpr jsize kSpecIntCount = 7;
constexpr jsize kSpecFloatCount = 33;

void throwJava(JNIEnv* env, const char* className, const std::string& message) {
    jclass type = env->FindClass(className);
    if (type != nullptr) {
        env->ThrowNew(type, message.c_str());
        env->DeleteLocalRef(type);
    }
}

void throwIllegalArgument(JNIEnv* env, const std::string& message) {
    throwJava(env, "java/lang/IllegalArgumentException", message);
}

void throwEngineFailure(JNIEnv* env, const char* stage, int32_t result, const af_engine* engine) {
    if (result == AF_ERR_CANCELLED) {
        throwJava(env, "java/util/concurrent/CancellationException", "AvatarForge generation was cancelled");
        return;
    }
    const char* detail = engine == nullptr ? nullptr : af_engine_last_error(engine);
    std::string message = std::string(stage) + " failed (af_result=" + std::to_string(result) + ")";
    if (detail != nullptr && detail[0] != '\0') {
        message += ": ";
        message += detail;
    }
    throwJava(env, "java/lang/IllegalStateException", message);
}

class UtfChars {
public:
    UtfChars(JNIEnv* env, jstring value) : env_(env), value_(value) {
        chars_ = value_ == nullptr ? nullptr : env_->GetStringUTFChars(value_, nullptr);
    }
    ~UtfChars() {
        if (chars_ != nullptr) env_->ReleaseStringUTFChars(value_, chars_);
    }
    const char* get() const { return chars_; }
private:
    JNIEnv* env_;
    jstring value_;
    const char* chars_ = nullptr;
};

struct CancelContext {
    JavaVM* vm = nullptr;
    JNIEnv* ownerEnv = nullptr;
    jobject token = nullptr;  // Global reference: callback may run on native worker threads.
    jmethodID method = nullptr;

    ~CancelContext() {
        if (ownerEnv != nullptr && token != nullptr) {
            ownerEnv->DeleteGlobalRef(token);
            token = nullptr;
        }
    }
};

struct WorkerJniEnv {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    ~WorkerJniEnv() {
        if (vm != nullptr && env != nullptr) vm->DetachCurrentThread();
    }
};

int shouldCancel(void* user) {
    auto* context = static_cast<CancelContext*>(user);
    if (context == nullptr || context->vm == nullptr || context->token == nullptr || context->method == nullptr) {
        return 1;
    }

    // JNIEnv is thread-local. Cache an attachment for the lifetime of each native
    // worker thread, then detach in its thread-local destructor.
    static thread_local WorkerJniEnv worker;
    if (worker.vm != context->vm) {
        if (worker.vm != nullptr && worker.env != nullptr) worker.vm->DetachCurrentThread();
        worker.vm = nullptr;
        worker.env = nullptr;
        JNIEnv* current = nullptr;
        const jint state = context->vm->GetEnv(reinterpret_cast<void**>(&current), JNI_VERSION_1_6);
        if (state == JNI_EDETACHED) {
            if (context->vm->AttachCurrentThread(&current, nullptr) != JNI_OK) return 1;
            worker.vm = context->vm;
            worker.env = current;
        } else if (state == JNI_OK && current != nullptr) {
            worker.env = current; // Already attached by the JVM; do not detach it.
        } else {
            return 1;
        }
    }

    JNIEnv* workerEnv = worker.env;
    if (workerEnv == nullptr) return 1;
    const jboolean cancelled = workerEnv->CallBooleanMethod(context->token, context->method);
    const bool hadException = workerEnv->ExceptionCheck();
    if (hadException) workerEnv->ExceptionClear();
    return hadException || cancelled == JNI_TRUE ? 1 : 0;
}

bool containsMinorMarker(const char* prompt) {
    static const std::unordered_set<std::string> markers = {
        "boy", "child", "girl", "kid", "minor", "schoolboy", "schoolgirl", "teen", "teenage", "underage",
    };
    std::string token;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(prompt); *p != '\0'; ++p) {
        if (std::isalpha(*p)) {
            token.push_back(static_cast<char>(std::tolower(*p)));
        } else {
            if (markers.find(token) != markers.end()) return true;
            token.clear();
        }
    }
    return markers.find(token) != markers.end();
}

bool validateParsedAdultMode(JNIEnv* env, const af_spec& parsed, jboolean adultModeConfirmed, const char* prompt) {
    if (parsed.outfit != AF_OUTFIT_NONE) return true;
    if (adultModeConfirmed != JNI_TRUE) {
        throwIllegalArgument(env, "Prompt resolves to an unclothed avatar; explicit adult-mode confirmation is required");
        return false;
    }
    if (!std::isfinite(parsed.age_years) || parsed.age_years < 18.0f) {
        throwIllegalArgument(env, "Unclothed avatar generation requires an adult age of 18 or older");
        return false;
    }
    if (containsMinorMarker(prompt)) {
        throwIllegalArgument(env, "Unclothed avatar generation rejects minor or teen prompt terms");
        return false;
    }
    return true;
}

bool readSpec(JNIEnv* env, jintArray ints, jfloatArray floats, jstring prompt, af_spec* out) {
    if (ints == nullptr || floats == nullptr || prompt == nullptr || out == nullptr) {
        throwIllegalArgument(env, "Avatar spec arrays and prompt must not be null");
        return false;
    }
    if (env->GetArrayLength(ints) != kSpecIntCount || env->GetArrayLength(floats) != kSpecFloatCount) {
        throwIllegalArgument(env, "Avatar spec packing does not match AvatarForge ABI v1");
        return false;
    }

    jint iv[kSpecIntCount]{};
    jfloat fv[kSpecFloatCount]{};
    env->GetIntArrayRegion(ints, 0, kSpecIntCount, iv);
    env->GetFloatArrayRegion(floats, 0, kSpecFloatCount, fv);
    if (env->ExceptionCheck()) return false;

    af_spec_init(out);
    out->sex = iv[1];
    out->build = iv[2];
    out->hair_style = iv[3];
    out->outfit = iv[4];
    out->footwear = iv[5];
    out->seed = iv[6];

    int i = 0;
    out->height_m = fv[i++];
    out->age_years = fv[i++];
    out->muscle = fv[i++];
    out->body_fat = fv[i++];
    out->bust = fv[i++];
    out->neck_cm = fv[i++];
    out->chest_cm = fv[i++];
    out->underbust_cm = fv[i++];
    out->waist_cm = fv[i++];
    out->hip_cm = fv[i++];
    out->thigh_cm = fv[i++];
    out->calf_cm = fv[i++];
    out->bicep_cm = fv[i++];
    out->forearm_cm = fv[i++];
    out->wrist_cm = fv[i++];
    out->shoulder_width_cm = fv[i++];
    out->inseam_cm = fv[i++];
    out->head_circumference_cm = fv[i++];
    for (int c = 0; c < 3; ++c) out->skin_tone[c] = fv[i++];
    out->skin_melanin = fv[i++];
    for (int c = 0; c < 3; ++c) out->hair_tone[c] = fv[i++];
    for (int c = 0; c < 3; ++c) out->cloth_tone[c] = fv[i++];
    for (int c = 0; c < 3; ++c) out->gear_tone[c] = fv[i++];
    out->cloth_roughness = fv[i++];
    out->gear_metalness = fv[i++];

    UtfChars promptChars(env, prompt);
    if (promptChars.get() == nullptr) return false;
    std::snprintf(out->prompt, sizeof(out->prompt), "%s", promptChars.get());
    return true;
}
}  // namespace

extern "C" JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_NativeMethods_abiVersion(JNIEnv*, jobject) {
    return af_engine_abi_version();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_NativeMethods_engineVersion(JNIEnv* env, jobject) {
    const char* version = af_engine_version();
    return env->NewStringUTF(version == nullptr ? "unknown" : version);
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_davemega_hub_avatarforge_NativeMethods_generate(
    JNIEnv* env,
    jobject,
    jintArray ints,
    jfloatArray floats,
    jstring prompt,
    jstring outputPath,
    jlong memoryBudgetBytes,
    jboolean adultModeConfirmed,
    jobject cancellationToken) {
    if (outputPath == nullptr || cancellationToken == nullptr || memoryBudgetBytes <= 0) {
        throwIllegalArgument(env, "output path, cancellation token, and positive memory budget are required");
        return nullptr;
    }

    af_spec spec{};
    if (!readSpec(env, ints, floats, prompt, &spec)) return nullptr;
    UtfChars promptChars(env, prompt);
    if (promptChars.get() == nullptr) return nullptr;

    UtfChars pathChars(env, outputPath);
    if (pathChars.get() == nullptr) return nullptr;
    const std::string path(pathChars.get());
    if (path.empty()) {
        throwIllegalArgument(env, "output path must not be empty");
        return nullptr;
    }

    jclass tokenClass = env->GetObjectClass(cancellationToken);
    if (tokenClass == nullptr) return nullptr;
    const jmethodID cancelMethod = env->GetMethodID(tokenClass, "shouldCancel", "()Z");
    env->DeleteLocalRef(tokenClass);
    if (cancelMethod == nullptr) return nullptr;

    CancelContext cancelContext{};
    cancelContext.ownerEnv = env;
    cancelContext.method = cancelMethod;
    if (env->GetJavaVM(&cancelContext.vm) != JNI_OK) {
        throwJava(env, "java/lang/IllegalStateException", "Unable to obtain JavaVM for cancellation callbacks");
        return nullptr;
    }
    cancelContext.token = env->NewGlobalRef(cancellationToken);
    if (cancelContext.token == nullptr) return nullptr;

    af_callbacks callbacks{};
    callbacks.user = &cancelContext;
    callbacks.should_cancel = shouldCancel;

    af_engine* rawEngine = nullptr;
    int32_t result = af_engine_create(&spec, &callbacks, &rawEngine);
    std::unique_ptr<af_engine, decltype(&af_engine_destroy)> engine(rawEngine, &af_engine_destroy);
    if (result != AF_OK || engine == nullptr) {
        throwEngineFailure(env, "af_engine_create", result, engine.get());
        return nullptr;
    }

    result = af_engine_set_memory_budget(engine.get(), static_cast<uint64_t>(memoryBudgetBytes));
    if (result != AF_OK) {
        throwEngineFailure(env, "af_engine_set_memory_budget", result, engine.get());
        return nullptr;
    }
    af_spec parsedSpec{};
    const int32_t promptMatched = af_engine_analyze_prompt(engine.get(), promptChars.get(), &parsedSpec);
    if (promptMatched < 0) {
        throwEngineFailure(env, "af_engine_analyze_prompt", promptMatched, engine.get());
        return nullptr;
    }
    if (!validateParsedAdultMode(env, parsedSpec, adultModeConfirmed, promptChars.get())) return nullptr;

    result = af_engine_run_all(engine.get());
    if (env->ExceptionCheck()) return nullptr;
    if (result != AF_OK) {
        throwEngineFailure(env, "af_engine_run_all", result, engine.get());
        return nullptr;
    }
    result = af_engine_apply_clip(engine.get(), AF_CLIP_REST, 0.0f);
    if (result != AF_OK) {
        throwEngineFailure(env, "af_engine_apply_clip(AF_CLIP_REST)", result, engine.get());
        return nullptr;
    }
    char meshReport[2048] = {};
    result = af_engine_validate_deformed_mesh(
        engine.get(), meshReport, static_cast<int32_t>(sizeof(meshReport)));
    if (result != AF_OK) {
        throwJava(env, "java/lang/IllegalStateException",
                  meshReport[0] ? meshReport : "Deformed mesh validation failed");
        return nullptr;
    }

    result = af_engine_export_glb(engine.get(), path.c_str());
    if (result != AF_OK) {
        throwEngineFailure(env, "af_engine_export_glb", result, engine.get());
        return nullptr;
    }

    af_stats stats{};
    result = af_engine_stats(engine.get(), &stats);
    if (result != AF_OK) {
        throwEngineFailure(env, "af_engine_stats", result, engine.get());
        return nullptr;
    }

    const jlong values[] = {
        static_cast<jlong>(stats.vertex_count),
        static_cast<jlong>(stats.index_count),
        static_cast<jlong>(stats.triangle_count),
        static_cast<jlong>(stats.submesh_count),
        static_cast<jlong>(stats.joint_count),
        static_cast<jlong>(stats.blendshape_count),
        static_cast<jlong>(stats.texture_bytes),
        static_cast<jlong>(stats.geometry_bytes),
        static_cast<jlong>(stats.peak_field_bytes),
        static_cast<jlong>(promptMatched),
        static_cast<jlong>(1),  // Post-skinning vertices and indexed submesh bounds passed validation.
    };
    jlongArray out = env->NewLongArray(static_cast<jsize>(sizeof(values) / sizeof(values[0])));
    if (out != nullptr) env->SetLongArrayRegion(out, 0, static_cast<jsize>(sizeof(values) / sizeof(values[0])), values);
    return out;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    gJavaVm = vm;
    af::setFallbackImageDecoder(&decodeViaAndroidBitmapFactory);
    return JNI_VERSION_1_6;
}


