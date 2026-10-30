#include <jni.h>

#include "afcore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>

namespace {
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
    JNIEnv* env;
    jobject token;
    jmethodID method;
};

int shouldCancel(void* user) {
    auto* context = static_cast<CancelContext*>(user);
    if (context == nullptr || context->env == nullptr || context->token == nullptr || context->method == nullptr) {
        return 0;
    }
    const jboolean cancelled = context->env->CallBooleanMethod(context->token, context->method);
    if (context->env->ExceptionCheck()) return 1;
    return cancelled == JNI_TRUE ? 1 : 0;
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

    CancelContext cancelContext{env, cancellationToken, cancelMethod};
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
    };
    jlongArray out = env->NewLongArray(static_cast<jsize>(sizeof(values) / sizeof(values[0])));
    if (out != nullptr) env->SetLongArrayRegion(out, 0, static_cast<jsize>(sizeof(values) / sizeof(values[0])), values);
    return out;
}
