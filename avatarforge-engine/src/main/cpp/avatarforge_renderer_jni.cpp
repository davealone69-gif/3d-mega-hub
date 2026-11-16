#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "afcore.h"
#include "render/af_render.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
/* Process-wide lifetime accounting. A session is counted once it has been handed to Java and
   again when its destructor has released the renderer, engine and ANativeWindow. created minus
   destroyed is therefore the number of native sessions Java still owns; a value that does not
   return to zero after every surface has gone is a native resource leak. */
std::atomic<int64_t> gSessionsCreated{0};
std::atomic<int64_t> gSessionsDestroyed{0};

using SteadyClock = std::chrono::steady_clock;
int64_t microsBetween(SteadyClock::time_point from, SteadyClock::time_point to) {
    return std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
}

struct NativeAvatarSession {
    ANativeWindow* window = nullptr;
    af_engine* engine = nullptr;
    af::Renderer* renderer = nullptr;
    int width = 0;
    int height = 0;
    af::CameraState camera{};
    af::CameraState initialCamera{};
    float minDistance = 0.05f;
    float maxDistance = 100.0f;
    std::string verificationReport;
    /* Measured with steady_clock in nativeCreate/nativeRender; -1 means "not measured". */
    int64_t importMicros = -1;        /* af_engine_import_glb()                              */
    int64_t poseMicros = -1;          /* initial AF_CLIP_REST for skinned imports            */
    int64_t rendererInitMicros = -1;  /* Renderer::create + resize + attach                   */
    int64_t createMicros = -1;        /* the whole nativeCreate call                          */
    int64_t firstFrameMicros = -1;    /* the first renderFrame(): shader/geometry/LOD upload  */
    int64_t framesRequested = 0;
    bool countedAsCreated = false;
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
        if (countedAsCreated) gSessionsDestroyed.fetch_add(1, std::memory_order_relaxed);
    }
};

/* Hands a fully constructed session to Java and records it in the lifetime counters. */
jlong publishSession(std::unique_ptr<NativeAvatarSession>& session) {
    session->countedAsCreated = true;
    gSessionsCreated.fetch_add(1, std::memory_order_relaxed);
    return reinterpret_cast<jlong>(session.release());
}

void throwState(JNIEnv* env, const std::string& message) {
    jclass type = env->FindClass("java/lang/IllegalStateException");
    if (type) {
        env->ThrowNew(type, message.c_str());
        env->DeleteLocalRef(type);
    }
}

/* Reports a C++ exception to Java without allocating native memory: the exception may well be
   std::bad_alloc. A Java exception that is already pending is the first cause and is kept. */
void throwNativeFailure(JNIEnv* env, const char* where, const char* detail) noexcept {
    if (env->ExceptionCheck()) return;
    char message[512];
    std::snprintf(message, sizeof(message), "%s failed with a native exception: %s",
                  where, (detail && *detail) ? detail : "no description");
    jclass type = env->FindClass("java/lang/IllegalStateException");
    if (type) {
        env->ThrowNew(type, message);
        env->DeleteLocalRef(type);
    }
}

/*
 * A C++ exception must never unwind into the JVM: that aborts the whole app. The engine's C API
 * already catches its own exceptions, but the renderer does not, and it allocates hundreds of
 * megabytes of geometry and level-of-detail buffers for a large avatar. Every entry point that
 * runs renderer code goes through this barrier. The exception becomes an IllegalStateException;
 * the Kotlin render thread reports it as the renderer failure and tears the session down.
 */
template <typename Result, typename Body>
Result jniBarrier(JNIEnv* env, const char* where, Result fallback, Body&& body) noexcept {
    try {
        return body();
    } catch (const std::bad_alloc&) {
        throwNativeFailure(env, where, "out of native memory (std::bad_alloc)");
    } catch (const std::exception& failure) {
        throwNativeFailure(env, where, failure.what());
    } catch (...) {
        throwNativeFailure(env, where, "unknown C++ exception");
    }
    return fallback;
}

jlong createImportedSession(JNIEnv* env, jobject surface, jstring glbPath, jint width, jint height);
jlong createGeneratedSession(JNIEnv* env, jobject surface, jint width, jint height, bool debugMagenta);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeCreate(
    JNIEnv* env, jobject, jobject surface, jstring glbPath, jint width, jint height) {
    return jniBarrier<jlong>(env, "AvatarForge renderer start", 0, [&]() -> jlong {
        return createImportedSession(env, surface, glbPath, width, height);
    });
}

namespace {
jlong createImportedSession(JNIEnv* env, jobject surface, jstring glbPath, jint width, jint height) {
    if (!surface || !glbPath || width <= 0 || height <= 0) {
        throwState(env, "AvatarForge renderer requires a Surface, GLB path and non-zero size");
        return 0;
    }
    /* Copy the path and give the JNI characters back at once, so that no later failure, a C++
       exception included, can leave them pinned. */
    const char* pathChars = env->GetStringUTFChars(glbPath, nullptr);
    if (!pathChars) return 0;
    std::string pathCopy;
    try {
        pathCopy = pathChars;
    } catch (...) {
        env->ReleaseStringUTFChars(glbPath, pathChars);
        throw;
    }
    env->ReleaseStringUTFChars(glbPath, pathChars);
    const char* path = pathCopy.c_str();
    const SteadyClock::time_point createStart = SteadyClock::now();

    std::unique_ptr<NativeAvatarSession> session(new (std::nothrow) NativeAvatarSession());
    if (!session) {
        throwState(env, "Unable to allocate AvatarForge render session");
        return 0;
    }

    session->window = ANativeWindow_fromSurface(env, surface);
    if (!session->window) {
        throwState(env, "Android could not provide a native window for the AvatarForge surface");
        return 0;
    }
    session->width = width;
    session->height = height;

    af_spec spec{};
    af_spec_init(&spec);
    const int32_t createResult = af_engine_create(&spec, nullptr, &session->engine);
    if (createResult != AF_OK || !session->engine) {
        throwState(env, "af_engine_create failed: " + std::to_string(createResult));
        return 0;
    }

    const SteadyClock::time_point importStart = SteadyClock::now();
    const int32_t importResult = af_engine_import_glb(session->engine, path);
    session->importMicros = microsBetween(importStart, SteadyClock::now());
    const std::string importError = af_engine_last_error(session->engine)
        ? af_engine_last_error(session->engine) : "";
    if (importResult != AF_OK) {
        throwState(env, "AvatarForge GLB import failed (" + std::to_string(importResult) +
                        "): " + importError);
        return 0;
    }

    /* Apply the initial skin pose before reading submesh bounds for camera framing.
       Static GLBs have no joints and keep their original import path. */
    if (af_engine_joint_count(session->engine) > 0) {
        const SteadyClock::time_point poseStart = SteadyClock::now();
        const int32_t poseResult = af_engine_apply_clip(session->engine, AF_CLIP_REST, 0.0f);
        session->poseMicros = microsBetween(poseStart, SteadyClock::now());
        if (poseResult != AF_OK) {
            const char* poseError = af_engine_last_error(session->engine);
            throwState(env, "Applying initial AF_CLIP_REST before GLES camera framing failed (" +
                            std::to_string(poseResult) + "): " +
                            (poseError ? poseError : ""));
            return 0;
        }
    }

    const int32_t submeshCount = af_engine_submesh_count(session->engine);
    const af_submesh* submeshes = af_engine_submeshes(session->engine);
    if (submeshCount <= 0 || !submeshes) {
        throwState(env, "AvatarForge imported no renderable submeshes");
        return 0;
    }
    bool allForeignSubmeshes = true;
    for (int32_t i = 0; i < submeshCount; ++i) {
        if (submeshes[i].region >= 0) {
            allForeignSubmeshes = false;
            break;
        }
    }

    af::RendererOptions options{};
    options.preferVulkan = false; // Samsung Galaxy S24 (SM-S921B) uses the dedicated GLES 3.2 path.
    options.quality = af::QualityPreset::HIGH;
    options.msaaSamples = 4;
    options.shadowMapSize = 2048;
    options.shadowSoftness = 1.65f;
    options.enableBloom = true;
    options.enableSss = true;
    if (allForeignSubmeshes) {
        // Standard glTF 2.0 assets (§3.4) face +Z; studio portrait key + fill lighting for yawDeg = 0.
        options.sunAzimuthDeg = 152.0f;
        options.sunElevationDeg = 28.0f;
        options.sunIntensity = 1.35f;
        options.environmentIntensity = 0.82f;
        options.exposure = 0.82f;
    } else {
        options.sunAzimuthDeg = 34.0f;
        options.sunElevationDeg = 28.0f;
        options.sunIntensity = 1.35f;
        options.environmentIntensity = 0.82f;
        options.exposure = 0.82f;
    }
    std::string rendererError;
    const SteadyClock::time_point rendererStart = SteadyClock::now();
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
    session->rendererInitMicros = microsBetween(rendererStart, SteadyClock::now());

    // Fit the imported asset, not a hard-coded 1.75 m avatar. This handles tiny props,
    // large scenes and GLBs whose origin is far from their actual geometry.
    float boundsMin[3] = { 1.0e30f, 1.0e30f, 1.0e30f };
    float boundsMax[3] = { -1.0e30f, -1.0e30f, -1.0e30f };
    for (int32_t i = 0; i < submeshCount; ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            boundsMin[axis] = std::min(boundsMin[axis], submeshes[i].bounds_min[axis]);
            boundsMax[axis] = std::max(boundsMax[axis], submeshes[i].bounds_max[axis]);
        }
    }
    const float cx = 0.5f * (boundsMin[0] + boundsMax[0]);
    const float cy = 0.5f * (boundsMin[1] + boundsMax[1]);
    const float cz = 0.5f * (boundsMin[2] + boundsMax[2]);
    const float dx = boundsMax[0] - boundsMin[0];
    const float dy = boundsMax[1] - boundsMin[1];
    const float dz = boundsMax[2] - boundsMin[2];
    const float radius = std::max(0.0001f, 0.5f * std::sqrt(dx*dx + dy*dy + dz*dz));
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float halfVertical = 28.0f * 3.14159265358979323846f / 360.0f;
    const float tanHalfV = std::max(0.01f, std::tan(halfVertical));
    const float tanHalfH = std::max(0.01f, tanHalfV * aspect);
    const float halfPlanExtent = std::max(0.0001f, 0.5f * std::max(dx, dz));
    const float halfHeightExtent = std::max(0.0001f, 0.5f * dy);
    const float fitDistH = halfPlanExtent / tanHalfH + 0.5f * dz;
    const float fitDistV = halfHeightExtent / tanHalfV + 0.5f * dz;
    af::CameraState camera{};
    camera.yawDeg = allForeignSubmeshes ? 0.0f : 180.0f;
    camera.targetX = cx;
    camera.targetY = cy;
    camera.targetZ = cz;
    camera.fovDeg = 28.0f;
    camera.distance = std::max(fitDistH, fitDistV) * 1.12f;
    session->camera = camera;
    session->initialCamera = camera;
    session->minDistance = std::max(0.02f, radius * 0.25f);
    session->maxDistance = std::max(session->minDistance * 8.0f, camera.distance * 6.0f);
    // buildCameraUbo derives near/far from the imported model bounds each frame.
    // Keep clipping-plane policy in one place instead of maintaining a second camera contract.
    session->renderer->setCamera(camera);
    session->createMicros = microsBetween(createStart, SteadyClock::now());
    return publishSession(session);
}

jlong createGeneratedSession(JNIEnv* env, jobject surface, jint width, jint height, bool debugMagenta) {
    if (!surface || width <= 0 || height <= 0) {
        throwState(env, "Generated-avatar verification requires a valid Surface and non-zero size");
        return 0;
    }
    std::unique_ptr<NativeAvatarSession> session(new (std::nothrow) NativeAvatarSession());
    if (!session) {
        throwState(env, "Unable to allocate generated-avatar native session");
        return 0;
    }
    session->window = ANativeWindow_fromSurface(env, surface);
    if (!session->window) {
        throwState(env, "Android could not provide a native window for the verification surface");
        return 0;
    }
    session->width = width;
    session->height = height;

    /* This exact engine handle is generated, validated, attached and rendered below. */
    af_spec spec{};
    af_spec_init(&spec);
    spec.seed = 1337;
    const int32_t createResult = af_engine_create(&spec, nullptr, &session->engine);
    if (createResult != AF_OK || !session->engine) {
        throwState(env, "af_engine_create failed: " + std::to_string(createResult));
        return 0;
    }

    static constexpr const char* kPrompt =
        "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans";
    af_spec resolved{};
    const int32_t matched = af_engine_analyze_prompt(session->engine, kPrompt, &resolved);
    if (matched < 0) {
        const char* error = af_engine_last_error(session->engine);
        throwState(env, std::string("Prompt analysis failed: ") + (error ? error : "unknown error"));
        return 0;
    }
    if (resolved.seed != 1337 || resolved.sex != AF_SEX_FEMALE ||
        resolved.build != AF_BUILD_ATHLETIC || resolved.hair_style != AF_HAIR_MEDIUM ||
        /* "black t-shirt and jeans" is the T-shirt outfit, which is built with jeans. The parser
           used to answer "jeans and jacket" here, and this check used to require that answer. */
        resolved.outfit != AF_OUTFIT_TSHIRT ||
        !std::isfinite(resolved.height_m) || std::fabs(resolved.height_m - 1.75f) > 0.001f) {
        throwState(env, "Prompt parser did not resolve the exact expected spec: seed=" +
            std::to_string(resolved.seed) + " sex=" + std::to_string(resolved.sex) +
            " build=" + std::to_string(resolved.build) + " hair=" +
            std::to_string(resolved.hair_style) + " outfit=" +
            std::to_string(resolved.outfit) + " height_m=" + std::to_string(resolved.height_m));
        return 0;
    }
    const int32_t generationResult = af_engine_run_all(session->engine);
    if (generationResult != AF_OK) {
        const char* error = af_engine_last_error(session->engine);
        throwState(env, "Procedural generation failed (" + std::to_string(generationResult) +
                        "): " + (error ? error : ""));
        return 0;
    }
    const int32_t poseResult = af_engine_apply_clip(session->engine, AF_CLIP_REST, 0.0f);
    if (poseResult != AF_OK) {
        const char* error = af_engine_last_error(session->engine);
        throwState(env, "Applying AF_CLIP_REST failed (" + std::to_string(poseResult) +
                        "): " + (error ? error : ""));
        return 0;
    }

    char meshReport[2048] = {};
    const int32_t meshResult = af_engine_validate_deformed_mesh(
        session->engine, meshReport, static_cast<int32_t>(sizeof(meshReport)));
    if (meshResult != AF_OK) {
        throwState(env, std::string("Generated mesh validation failed: ") + meshReport);
        return 0;
    }
    af_stats engineStats{};
    const int32_t statsResult = af_engine_stats(session->engine, &engineStats);
    if (statsResult != AF_OK) {
        throwState(env, "af_engine_stats failed: " + std::to_string(statsResult));
        return 0;
    }
    const af_vertex* vertices = af_engine_vertices(session->engine);
    const int32_t vertexCount = af_engine_vertex_count(session->engine);
    float minY = std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    for (int32_t i = 0; i < vertexCount; ++i) {
        minY = std::min(minY, vertices[i].pos[1]);
        maxY = std::max(maxY, vertices[i].pos[1]);
    }
    const float generatedHeight = maxY - minY;
    if (engineStats.triangle_count <= 0 || engineStats.vertex_count <= 0 ||
        engineStats.joint_count <= 0 || engineStats.blendshape_count <= 0 ||
        !std::isfinite(generatedHeight) || generatedHeight <= 0.0f) {
        throwState(env, "Generated-avatar engine returned invalid live geometry metrics: triangles=" +
            std::to_string(engineStats.triangle_count) + " vertices=" +
            std::to_string(engineStats.vertex_count) + " joints=" +
            std::to_string(engineStats.joint_count) + " blendshapes=" +
            std::to_string(engineStats.blendshape_count) + " height_m=" +
            std::to_string(generatedHeight) + "; mesh=" + meshReport);
        return 0;
    }

    af::RendererOptions options{};
    options.preferVulkan = false;
    options.quality = af::QualityPreset::HIGH;
    options.msaaSamples = 4;
    options.shadowMapSize = 2048;
    options.shadowSoftness = 1.65f;
    options.enableBloom = !debugMagenta;
    options.enableSss = true;
    options.sunAzimuthDeg = 34.0f;
    options.sunElevationDeg = 28.0f;
    options.sunIntensity = 1.35f;
    options.environmentIntensity = 0.82f;
    options.exposure = 0.82f;
    options.groundPlane = false; // Pixel threshold must measure the avatar, not a studio floor.
    options.verifyFramePixels = true;
    options.debugMagenta = debugMagenta;
    std::string rendererError;
    session->renderer = af::Renderer::create(session->window, options, &rendererError);
    if (!session->renderer) {
        throwState(env, "Generated-avatar GLES renderer creation failed: " + rendererError);
        return 0;
    }
    const af::RendererInfo rendererInfo = session->renderer->info();
    if (rendererInfo.backend != "gles31") {
        throwState(env, "Generated-avatar test requires GLES 3.1, got backend=" + rendererInfo.backend);
        return 0;
    }
    if (!session->renderer->resize(width, height)) {
        throwState(env, "Generated-avatar renderer rejected the verification surface dimensions");
        return 0;
    }
    session->renderer->attachEngine(session->engine);
    session->renderer->invalidateMesh();

    const int32_t submeshCount = af_engine_submesh_count(session->engine);
    const af_submesh* submeshes = af_engine_submeshes(session->engine);
    if (submeshCount <= 0 || !submeshes) {
        throwState(env, "Procedural generation produced no renderable submeshes");
        return 0;
    }
    float boundsMin[3] = {1.0e30f, 1.0e30f, 1.0e30f};
    float boundsMax[3] = {-1.0e30f, -1.0e30f, -1.0e30f};
    for (int32_t i = 0; i < submeshCount; ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            boundsMin[axis] = std::min(boundsMin[axis], submeshes[i].bounds_min[axis]);
            boundsMax[axis] = std::max(boundsMax[axis], submeshes[i].bounds_max[axis]);
        }
    }
    const float cx = 0.5f * (boundsMin[0] + boundsMax[0]);
    const float cy = 0.5f * (boundsMin[1] + boundsMax[1]);
    const float cz = 0.5f * (boundsMin[2] + boundsMax[2]);
    const float dx = boundsMax[0] - boundsMin[0];
    const float dy = boundsMax[1] - boundsMin[1];
    const float dz = boundsMax[2] - boundsMin[2];
    const float radius = std::max(0.0001f, 0.5f * std::sqrt(dx*dx + dy*dy + dz*dz));
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float halfVertical = 32.0f * 3.14159265358979323846f / 360.0f;
    const float halfHorizontal = std::atan(std::tan(halfVertical) * aspect);
    const float limitingHalfAngle = std::max(0.01f, std::min(halfVertical, halfHorizontal));
    af::CameraState camera{};
    camera.targetX = cx;
    camera.targetY = cy;
    camera.targetZ = cz;
    camera.fovDeg = 32.0f;
    camera.distance = radius / std::sin(limitingHalfAngle) * 1.15f;
    session->renderer->setCamera(camera);

    session->verificationReport = std::string("prompt=") + kPrompt +
        " seed=1337 parser_matches=" + std::to_string(matched) +
        " resolved_sex=" + std::to_string(resolved.sex) +
        " resolved_build=" + std::to_string(resolved.build) +
        " resolved_hair=" + std::to_string(resolved.hair_style) +
        " resolved_outfit=" + std::to_string(resolved.outfit) +
        " renderer_backend=" + rendererInfo.backend +
        " height_m=" + std::to_string(generatedHeight) +
        " blendshapes=" + std::to_string(engineStats.blendshape_count) +
        " " + meshReport;
    return publishSession(session);
}
} // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeCreateGenerated(
    JNIEnv* env, jobject, jobject surface, jint width, jint height) {
    return jniBarrier<jlong>(env, "Generated-avatar renderer start", 0, [&]() -> jlong {
        return createGeneratedSession(env, surface, width, height, false);
    });
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeCreateGeneratedDebug(
    JNIEnv* env, jobject, jobject surface, jint width, jint height, jboolean debugMagenta) {
    return jniBarrier<jlong>(env, "Generated-avatar renderer start", 0, [&]() -> jlong {
        return createGeneratedSession(env, surface, width, height, debugMagenta == JNI_TRUE);
    });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeRenderGenerated(
    JNIEnv* env, jobject, jlong handle, jfloat deltaSeconds) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    return jniBarrier<jboolean>(env, "Generated-avatar frame", JNI_FALSE, [&]() -> jboolean {
        session->renderer->renderFrame(deltaSeconds);
        return session->renderer->stats().framePresented ? JNI_TRUE : JNI_FALSE;
    });
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeGetVerificationStats(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || !session->engine) return nullptr;
    const af::RendererStats rs = session->renderer->stats();
    af_stats es{};
    af_engine_stats(session->engine, &es);
    const jint values[16] = {
        rs.frames, rs.drawCalls, rs.triangles, rs.vertices,
        rs.framePresented ? 1 : 0, rs.boundsInFrustum ? 1 : 0,
        rs.readbackValid ? 1 : 0, rs.readbackNonBlackPixels,
        rs.readbackWidth, rs.readbackHeight, es.joint_count, es.blendshape_count,
        rs.presentedFrames, rs.boundsAllCornersInDepthRange ? 1 : 0,
        rs.firstOutsideCorner, rs.outsideCorners
    };
    jintArray result = env->NewIntArray(16);
    if (result) env->SetIntArrayRegion(result, 0, 16, values);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeGetVerificationReport(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return env->NewStringUTF("invalid native verification session");
    const af::RendererStats rs = session->renderer->stats();
    char details[1024];
    std::snprintf(details, sizeof(details),
        " framePresented=%d presentedFrames=%d boundsInFrustum=%d allDepthCornersInRange=%d "
        "firstOutsideCorner=%d outsideCorners=%d firstOutsideView=(%.6f,%.6f,%.6f) "
        "viewDepth=[%.6f,%.6f] clip=[%.6f,%.6f] "
        "readbackValid=%d readbackNonBlackPixels=%d readbackSize=%dx%d glError=0x%x eglError=0x%x",
        rs.framePresented ? 1 : 0, rs.presentedFrames, rs.boundsInFrustum ? 1 : 0,
        rs.boundsAllCornersInDepthRange ? 1 : 0, rs.firstOutsideCorner, rs.outsideCorners,
        rs.firstOutsideViewX, rs.firstOutsideViewY, rs.firstOutsideViewZ,
        rs.minViewDepth, rs.maxViewDepth, rs.nearPlane, rs.farPlane,
        rs.readbackValid ? 1 : 0, rs.readbackNonBlackPixels,
        rs.readbackWidth, rs.readbackHeight, rs.glError, rs.eglError);
    return env->NewStringUTF((session->verificationReport + details).c_str());
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeGetReadbackPixels(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return nullptr;
    const std::vector<uint8_t> pixels = session->renderer->readbackPixels();
    if (pixels.empty() || pixels.size() > static_cast<size_t>(std::numeric_limits<jsize>::max())) return nullptr;
    jbyteArray result = env->NewByteArray(static_cast<jsize>(pixels.size()));
    if (result) env->SetByteArrayRegion(result, 0, static_cast<jsize>(pixels.size()),
                                        reinterpret_cast<const jbyte*>(pixels.data()));
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeDestroyGenerated(
    JNIEnv*, jobject, jlong handle) {
    delete reinterpret_cast<NativeAvatarSession*>(handle);
}
/* Fault injection for the instrumented test of the exception barrier. Real C++ exceptions are
   thrown inside the same jniBarrier that the renderer entry points use: kind 1 is a
   std::runtime_error, 2 a std::bad_alloc, 3 a non-standard exception, anything else returns 7. */
extern "C" JNIEXPORT jint JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeBarrierSelfTest(
    JNIEnv* env, jobject, jint kind) {
    return jniBarrier<jint>(env, "Barrier self-test", -1, [&]() -> jint {
        if (kind == 1) throw std::runtime_error("injected std::runtime_error");
        if (kind == 2) throw std::bad_alloc();
        if (kind == 3) throw 42;
        return 7;
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeResize(
    JNIEnv* env, jobject, jlong handle, jint width, jint height) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || width <= 0 || height <= 0) return;
    jniBarrier<int>(env, "AvatarForge renderer resize", 0, [&]() -> int {
        session->width = width;
        session->height = height;
        session->renderer->resize(width, height);
        return 0;
    });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeRender(
    JNIEnv* env, jobject, jlong handle, jfloat deltaSeconds) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    return jniBarrier<jboolean>(env, "AvatarForge frame", JNI_FALSE, [&]() -> jboolean {
        if (session->framesRequested++ == 0) {
            const SteadyClock::time_point firstStart = SteadyClock::now();
            session->renderer->renderFrame(deltaSeconds);
            session->firstFrameMicros = microsBetween(firstStart, SteadyClock::now());
        } else {
            session->renderer->renderFrame(deltaSeconds);
        }
        return session->renderer->stats().framePresented ? JNI_TRUE : JNI_FALSE;
    });
}

/*
 * Measured renderer metrics for the diagnostics panel and the persisted report. The element order
 * is a contract with AvatarForgeSurfaceView.Metrics; element 0 is the layout version and the
 * Kotlin side refuses any other version or length instead of guessing. Durations are microseconds
 * (-1 = not measured), byte counts are the sizes requested from the driver, and nothing here is
 * derived from the caller's frame delta.
 */
extern "C" JNIEXPORT jlongArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetMetrics(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || !session->engine) return nullptr;
    const af::RendererStats stats = session->renderer->stats();
    const auto micros = [](float milliseconds) { return static_cast<jlong>(std::llround(static_cast<double>(milliseconds) * 1000.0)); };
    const jlong values[32] = {
        1,                                                        /*  0 layout version            */
        static_cast<jlong>(session->importMicros),                /*  1                           */
        static_cast<jlong>(session->poseMicros),                  /*  2                           */
        static_cast<jlong>(session->rendererInitMicros),          /*  3                           */
        static_cast<jlong>(session->createMicros),                /*  4                           */
        static_cast<jlong>(session->firstFrameMicros),            /*  5                           */
        micros(stats.lastCpuMs),                                  /*  6                           */
        micros(stats.lastSwapMs),                                 /*  7                           */
        micros(stats.presentIntervalMs),                          /*  8                           */
        static_cast<jlong>(stats.vertices),                       /*  9 uploaded vertices         */
        static_cast<jlong>(stats.triangles),                      /* 10 triangles drawn this frame */
        static_cast<jlong>(stats.drawCalls),                      /* 11                           */
        static_cast<jlong>(stats.textureBytes),                   /* 12                           */
        static_cast<jlong>(stats.geometryBytes),                  /* 13                           */
        static_cast<jlong>(stats.renderTargetBytes),              /* 14                           */
        static_cast<jlong>(stats.msaaSamples),                    /* 15                           */
        static_cast<jlong>(stats.shadowMapSize),                  /* 16                           */
        static_cast<jlong>(stats.bloomLevels),                    /* 17                           */
        static_cast<jlong>(stats.qualityLevel),                   /* 18                           */
        static_cast<jlong>(stats.glErrorFrames),                  /* 19                           */
        static_cast<jlong>(stats.lastGlError),                    /* 20                           */
        static_cast<jlong>(stats.eglErrorFrames),                 /* 21                           */
        static_cast<jlong>(stats.failedDrawCalls),                /* 22                           */
        static_cast<jlong>(std::llround(static_cast<double>(session->camera.yawDeg) * 1000.0)),    /* 23 */
        static_cast<jlong>(std::llround(static_cast<double>(session->camera.pitchDeg) * 1000.0)),  /* 24 */
        static_cast<jlong>(std::llround(static_cast<double>(session->camera.distance) * 1.0e6)),   /* 25 */
        static_cast<jlong>(af_engine_vertex_count(session->engine)),                               /* 26 */
        static_cast<jlong>(af_engine_index_count(session->engine) / 3),                            /* 27 */
        static_cast<jlong>(af_engine_submesh_count(session->engine)),                              /* 28 */
        static_cast<jlong>(af_engine_joint_count(session->engine)),                                /* 29 */
        static_cast<jlong>(gSessionsCreated.load(std::memory_order_relaxed)),                      /* 30 */
        static_cast<jlong>(gSessionsDestroyed.load(std::memory_order_relaxed))                     /* 31 */
    };
    jlongArray result = env->NewLongArray(32);
    if (result) env->SetLongArrayRegion(result, 0, 32, values);
    return result;
}

/* Process-wide {created, destroyed} native session counters; valid with no live session. */
extern "C" JNIEXPORT jlongArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeRendererRuntime_nativeSessionCounters(JNIEnv* env, jobject) {
    const jlong values[2] = {
        static_cast<jlong>(gSessionsCreated.load(std::memory_order_relaxed)),
        static_cast<jlong>(gSessionsDestroyed.load(std::memory_order_relaxed))
    };
    jlongArray result = env->NewLongArray(2);
    if (result) env->SetLongArrayRegion(result, 0, 2, values);
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetInfo(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return env->NewStringUTF("invalid|no renderer session");
    const af::RendererInfo info = session->renderer->info();
    const std::string value = info.backend + "|" + info.apiVersion + "|" + info.deviceName;
    return env->NewStringUTF(value.c_str());
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetStats(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return nullptr;
    const af::RendererStats stats = session->renderer->stats();
    const jint values[9] = {
        static_cast<jint>(stats.frames),
        static_cast<jint>(stats.drawCalls),
        static_cast<jint>(stats.triangles),
        stats.framePresented ? 1 : 0,
        static_cast<jint>(stats.presentedFrames),
        stats.boundsInFrustum ? 1 : 0,
        static_cast<jint>(stats.outsideCorners),
        static_cast<jint>(stats.glError),
        static_cast<jint>(stats.eglError)
    };
    jintArray result = env->NewIntArray(9);
    if (result) env->SetIntArrayRegion(result, 0, 9, values);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeOrbitCamera(
    JNIEnv*, jobject, jlong handle, jfloat deltaYawDeg, jfloat deltaPitchDeg, jfloat zoomScale) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    if (std::isfinite(deltaYawDeg)) {
        session->camera.yawDeg = std::fmod(session->camera.yawDeg + deltaYawDeg, 360.0f);
    }
    if (std::isfinite(deltaPitchDeg)) {
        session->camera.pitchDeg = std::max(-85.0f, std::min(85.0f, session->camera.pitchDeg + deltaPitchDeg));
    }
    if (std::isfinite(zoomScale) && zoomScale > 0.0f) {
        session->camera.distance = std::max(session->minDistance,
                                            std::min(session->maxDistance, session->camera.distance * zoomScale));
    }
    session->renderer->setCamera(session->camera);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeResetCamera(
    JNIEnv*, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    session->camera = session->initialCamera;
    session->renderer->setCamera(session->camera);
}

/*
 * Frames the face (focus 1) or the whole model again (focus 0). The face is where the face rig
 * says it is, so this answers false for a model without one. The view direction is set to the
 * front; the user can orbit and zoom from there as before.
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeFocusCamera(
    JNIEnv*, jobject, jlong handle, jint focus) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    if (focus != 1) {
        session->camera = session->initialCamera;
        session->renderer->setCamera(session->camera);
        return JNI_TRUE;
    }
    if (!session->engine || af_engine_face_kernel_count(session->engine) <= 0) return JNI_FALSE;
    const float none[1] = {0.0f};
    float block[12 + 16] = {0};
    af_engine_pack_face_block(session->engine, none, 1, block, 1);
    /* The box spans the head from side to side and from under the chin to over the crown. In
       depth it is the whole half-space in front of the head, so depth says nothing: the orbit
       target keeps the depth of the model's centre. */
    const float width = block[4] - block[0], height = block[5] - block[1];
    if (!(width > 0.0f && height > 0.0f) || session->width <= 0 || session->height <= 0) return JNI_FALSE;
    af::CameraState camera = session->initialCamera;
    camera.targetX = 0.5f * (block[0] + block[4]);
    camera.targetY = 0.5f * (block[1] + block[5]);
    camera.pitchDeg = 2.0f;
    const float aspect = static_cast<float>(session->width) / static_cast<float>(session->height);
    const float tanHalfV = std::max(0.01f, std::tan(camera.fovDeg * 3.14159265358979323846f / 360.0f));
    const float tanHalfH = std::max(0.01f, tanHalfV * aspect);
    /* Head and shoulders: the face box takes a little over half of the smaller screen dimension. */
    const float fit = std::max(0.5f * height / tanHalfV, 0.5f * width / tanHalfH) * 1.7f;
    camera.distance = std::max(session->minDistance, std::min(session->maxDistance, fit));
    session->camera = camera;
    session->renderer->setCamera(session->camera);
    return JNI_TRUE;
}

/*
 * Built-in animation of the model in this session. The list is what the engine can play on the
 * skeleton it holds (af_engine_animation_*): every clip of an avatar this engine generated or
 * exported, nothing for a file whose skeleton it did not adopt. It is empty too when the backend
 * cannot skin on the GPU, because the alternative - re-posing and re-uploading every vertex each
 * frame - is not usable on a million-triangle avatar. All of these run on the render thread.
 */
extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetAnimations(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    jclass stringClass = env->FindClass("java/lang/String");
    if (!stringClass) return nullptr;
    int count = 0;
    if (session && session->engine && session->renderer &&
        session->renderer->supportsSkinnedPlayback() &&
        af_engine_skin_joint_count(session->engine) > 0) {
        count = std::max(0, af_engine_animation_count(session->engine));
    }
    jobjectArray result = env->NewObjectArray(count, stringClass, nullptr);
    if (!result) return nullptr;
    for (int i = 0; i < count; ++i) {
        char name[96] = {0};
        if (af_engine_animation_name(session->engine, i, name, static_cast<int32_t>(sizeof(name))) <= 0) {
            std::snprintf(name, sizeof(name), "animation %d", i + 1);
        }
        const float duration = af_engine_animation_duration(session->engine, i);
        const std::string entry = std::string(name) + "|" + std::to_string(std::isfinite(duration) ? duration : 0.0f);
        jstring text = env->NewStringUTF(entry.c_str());
        if (!text) return result;
        env->SetObjectArrayElement(result, i, text);
        env->DeleteLocalRef(text);
    }
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetAnimation(
    JNIEnv* env, jobject, jlong handle, jint animation, jfloat speed, jboolean loop) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    return jniBarrier<jboolean>(env, "AvatarForge animation", JNI_FALSE, [&]() -> jboolean {
        return session->renderer->setAnimation(static_cast<int>(animation), speed, loop == JNI_TRUE)
            ? JNI_TRUE : JNI_FALSE;
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetAnimationPlayback(
    JNIEnv*, jobject, jlong handle, jfloat speed, jboolean loop, jboolean paused) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    session->renderer->setAnimationPlayback(speed, loop == JNI_TRUE, paused == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSeekAnimation(
    JNIEnv*, jobject, jlong handle, jfloat seconds) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    session->renderer->seekAnimation(seconds);
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetAnimationTime(
    JNIEnv*, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return 0.0f;
    return session->renderer->animationTime();
}

/*
 * What the list of nativeGetAnimations cannot say, five ints per animation in the same order:
 * looping (the last pose is the first again), source (0 built in, 1 retargeted from a file),
 * moves_root, travels (the source walked away and the drift was taken out), frames.
 */
extern "C" JNIEXPORT jintArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetAnimationInfo(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    int count = 0;
    if (session && session->engine && session->renderer &&
        session->renderer->supportsSkinnedPlayback() &&
        af_engine_skin_joint_count(session->engine) > 0) {
        count = std::max(0, af_engine_animation_count(session->engine));
    }
    std::vector<jint> values(static_cast<size_t>(count) * 5, 0);
    for (int i = 0; i < count; ++i) {
        af_animation_info info;
        if (af_engine_animation_info(session->engine, i, &info) != AF_OK) continue;
        jint* out = &values[static_cast<size_t>(i) * 5];
        out[0] = info.looping;
        out[1] = info.source;
        out[2] = info.moves_root;
        out[3] = info.travels;
        out[4] = info.frames;
    }
    jintArray result = env->NewIntArray(static_cast<jsize>(values.size()));
    if (result && !values.empty()) env->SetIntArrayRegion(result, 0, static_cast<jsize>(values.size()), values.data());
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeCrossFadeAnimation(
    JNIEnv* env, jobject, jlong handle, jint animation, jfloat fadeSeconds, jfloat speed, jboolean loop) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    return jniBarrier<jboolean>(env, "AvatarForge animation crossfade", JNI_FALSE, [&]() -> jboolean {
        return session->renderer->crossFadeToAnimation(static_cast<int>(animation), fadeSeconds, speed, loop == JNI_TRUE)
            ? JNI_TRUE : JNI_FALSE;
    });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetAnimationOverlay(
    JNIEnv* env, jobject, jlong handle, jint animation, jint mask, jfloat weight, jfloat speed, jboolean loop,
    jfloat fadeSeconds) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    return jniBarrier<jboolean>(env, "AvatarForge animation overlay", JNI_FALSE, [&]() -> jboolean {
        return session->renderer->setAnimationOverlay(static_cast<int>(animation), static_cast<int>(mask), weight, speed,
                                                      loop == JNI_TRUE, fadeSeconds)
            ? JNI_TRUE : JNI_FALSE;
    });
}

/*
 * Playback as the renderer has it, 18 floats: animation, time, duration, speed, loop, paused,
 * finished, loops, fading, fadingFrom, fadeProgress, transitions, overlayAnimation, overlayMask,
 * overlayWeight, overlayTime, layers of the last frame, face kernels of the last frame.
 */
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetAnimationState(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    jfloat values[18] = {-1.0f, 0, 0, 1.0f, 1.0f, 0, 0, 0, 0, -1.0f, 1.0f, 0, -1.0f, 0, 0, 0, 0, 0};
    if (session && session->renderer) {
        const af::AnimationPlaybackState state = session->renderer->animationState();
        values[0] = static_cast<jfloat>(state.animation);
        values[1] = state.time;
        values[2] = state.duration;
        values[3] = state.speed;
        values[4] = state.loop ? 1.0f : 0.0f;
        values[5] = state.paused ? 1.0f : 0.0f;
        values[6] = state.finished ? 1.0f : 0.0f;
        values[7] = static_cast<jfloat>(state.loops);
        values[8] = state.fading ? 1.0f : 0.0f;
        values[9] = static_cast<jfloat>(state.fadingFrom);
        values[10] = state.fadeProgress;
        values[11] = static_cast<jfloat>(state.transitions);
        values[12] = static_cast<jfloat>(state.overlayAnimation);
        values[13] = static_cast<jfloat>(state.overlayMask);
        values[14] = state.overlayWeight;
        values[15] = state.overlayTime;
        values[16] = static_cast<jfloat>(state.layers);
        values[17] = static_cast<jfloat>(session->renderer->faceKernelsActive());
    }
    jfloatArray result = env->NewFloatArray(18);
    if (result) env->SetFloatArrayRegion(result, 0, 18, values);
    return result;
}

/*
 * The face. nativeGetFaceChannels: the names of the 52 channels, or nothing when this model's
 * face cannot be animated (a file from elsewhere, an avatar exported before the face rig was
 * written into the file, or a backend without the face block).
 */
extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetFaceChannels(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    jclass stringClass = env->FindClass("java/lang/String");
    if (!stringClass) return nullptr;
    char names[64][64];
    int count = 0;
    if (session && session->engine && session->renderer && session->renderer->supportsFaceAnimation()) {
        count = std::max(0, std::min(64, af_engine_blendshape_names(names, 64)));
    }
    jobjectArray result = env->NewObjectArray(count, stringClass, nullptr);
    if (!result) return nullptr;
    for (int i = 0; i < count; ++i) {
        names[i][63] = 0;
        jstring text = env->NewStringUTF(names[i]);
        if (!text) return result;
        env->SetObjectArrayElement(result, i, text);
        env->DeleteLocalRef(text);
    }
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetFaceWeights(
    JNIEnv* env, jobject, jlong handle, jfloatArray weights) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || !weights) return JNI_FALSE;
    jfloat values[64] = {0};
    const jsize count = std::min<jsize>(64, env->GetArrayLength(weights));
    if (count > 0) env->GetFloatArrayRegion(weights, 0, count, values);
    if (env->ExceptionCheck()) return JNI_FALSE;
    return jniBarrier<jboolean>(env, "AvatarForge face weights", JNI_FALSE, [&]() -> jboolean {
        return session->renderer->setFaceWeights(values, static_cast<int>(count)) ? JNI_TRUE : JNI_FALSE;
    });
}

/*
 * Adds the animations of another humanoid GLB to the model of this session, retargeted onto its
 * skeleton (af_engine_import_motions). Runs on the render thread, between frames: the engine
 * belongs to it. The answer is one line,
 *   rc|source joints|recognised|mapped|animations in the file|added|compatible|message
 * and the caller reads the animation list again when rc is 0.
 */
extern "C" JNIEXPORT jstring JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeImportMotions(
    JNIEnv* env, jobject, jlong handle, jstring path) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    af_motion_import_report report;
    std::memset(&report, 0, sizeof(report));
    int32_t rc = AF_ERR_INVALID_ARG;
    if (!session || !session->engine || !session->renderer || !path) {
        std::snprintf(report.message, sizeof(report.message), "The renderer is not running.");
    } else if (!session->renderer->supportsSkinnedPlayback()) {
        std::snprintf(report.message, sizeof(report.message), "This device's renderer cannot play skeletal animation.");
        rc = AF_ERR_UNSUPPORTED;
    } else {
        const char* chars = env->GetStringUTFChars(path, nullptr);
        if (chars) {
            rc = jniBarrier<int32_t>(env, "AvatarForge motion import", static_cast<int32_t>(AF_ERR_INTERNAL), [&]() -> int32_t {
                return af_engine_import_motions(session->engine, chars, &report);
            });
            env->ReleaseStringUTFChars(path, chars);
        } else {
            std::snprintf(report.message, sizeof(report.message), "The file path could not be read.");
        }
    }
    if (env->ExceptionCheck()) return nullptr;
    report.message[sizeof(report.message) - 1] = 0;
    /* The separator must not occur in the message, and NewStringUTF wants plain ASCII or valid
       modified UTF-8: file names from elsewhere are not to be trusted with either. */
    for (char* c = report.message; *c; ++c) {
        if (*c == '|' || static_cast<unsigned char>(*c) < 0x20 || static_cast<unsigned char>(*c) >= 0x7f) *c = '?';
    }
    const std::string line = std::to_string(rc) + "|" + std::to_string(report.source_joints) + "|" +
        std::to_string(report.recognised_joints) + "|" + std::to_string(report.mapped_joints) + "|" +
        std::to_string(report.animations_in_file) + "|" + std::to_string(report.animations_added) + "|" +
        std::to_string(report.compatible) + "|" + report.message;
    return env->NewStringUTF(line.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeDestroy(
    JNIEnv*, jobject, jlong handle) {
    delete reinterpret_cast<NativeAvatarSession*>(handle);
}

