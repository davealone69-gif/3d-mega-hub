#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "afcore.h"
#include "render/af_render.h"

#include <memory>
#include <new>
#include <string>

namespace {
struct NativeAvatarSession {
    ANativeWindow* window = nullptr;
    af_engine* engine = nullptr;
    af::Renderer* renderer = nullptr;
    int width = 0;
    int height = 0;
    ~NativeAvatarSession() {
        // Renderer may still reference both the engine buffers and the native window.
        if (renderer) {
            renderer->waitIdle();
            renderer->attachEngine(nullptr);
            delete renderer;
            renderer = nullptr;
        }
        if (engine) {
            af_engine_destroy(engine);
            engine = nullptr;
        }
        if (window) {
            ANativeWindow_release(window);
            window = nullptr;
        }
    }
};

void throwState(JNIEnv* env, const std::string& message) {
    jclass type = env->FindClass("java/lang/IllegalStateException");
    if (type) {
        env->ThrowNew(type, message.c_str());
        env->DeleteLocalRef(type);
    }
}
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeCreate(
    JNIEnv* env, jobject, jobject surface, jstring glbPath, jint width, jint height) {
    if (!surface || !glbPath || width <= 0 || height <= 0) {
        throwState(env, "AvatarForge renderer requires a Surface, GLB path and non-zero size");
        return 0;
    }
    const char* path = env->GetStringUTFChars(glbPath, nullptr);
    if (!path) return 0;

    std::unique_ptr<NativeAvatarSession> session(new (std::nothrow) NativeAvatarSession());
    if (!session) {
        env->ReleaseStringUTFChars(glbPath, path);
        throwState(env, "Unable to allocate AvatarForge render session");
        return 0;
    }

    session->window = ANativeWindow_fromSurface(env, surface);
    if (!session->window) {
        env->ReleaseStringUTFChars(glbPath, path);
        throwState(env, "Android could not provide a native window for the AvatarForge surface");
        return 0;
    }
    session->width = width;
    session->height = height;

    af_spec spec{};
    af_spec_init(&spec);
    const int32_t createResult = af_engine_create(&spec, nullptr, &session->engine);
    if (createResult != AF_OK || !session->engine) {
        env->ReleaseStringUTFChars(glbPath, path);
        throwState(env, "af_engine_create failed: " + std::to_string(createResult));
        return 0;
    }

    const int32_t importResult = af_engine_import_glb(session->engine, path);
    const std::string importError = af_engine_last_error(session->engine)
        ? af_engine_last_error(session->engine) : "";
    env->ReleaseStringUTFChars(glbPath, path);
    if (importResult != AF_OK) {
        throwState(env, "AvatarForge GLB import failed (" + std::to_string(importResult) +
                        "): " + importError);
        return 0;
    }

    af::RendererOptions options{};
    options.preferVulkan = false; // Target handset uses the GLES backend.
    options.quality = af::QualityPreset::MEDIUM;
    options.msaaSamples = 2;
    options.enableBloom = false;
    std::string rendererError;
    session->renderer = af::Renderer::create(session->window, options, &rendererError);
    if (!session->renderer) {
        throwState(env, "AvatarForge GLES renderer creation failed: " + rendererError);
        return 0;
    }
    if (!session->renderer->resize(width, height)) {
        throwState(env, "AvatarForge renderer rejected the Android surface dimensions");
        return 0;
    }
    session->renderer->attachEngine(session->engine);
    session->renderer->invalidateMesh();
    return reinterpret_cast<jlong>(session.release());
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeResize(
    JNIEnv*, jobject, jlong handle, jint width, jint height) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || width <= 0 || height <= 0) return;
    session->width = width;
    session->height = height;
    session->renderer->resize(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeRender(
    JNIEnv*, jobject, jlong handle, jfloat deltaSeconds) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (session && session->renderer) session->renderer->renderFrame(deltaSeconds);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeDestroy(
    JNIEnv*, jobject, jlong handle) {
    delete reinterpret_cast<NativeAvatarSession*>(handle);
}
