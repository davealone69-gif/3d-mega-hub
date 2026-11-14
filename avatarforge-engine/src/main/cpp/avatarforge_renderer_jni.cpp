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
#include <memory>
#include <limits>
#include <new>
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
    const SteadyClock::time_point createStart = SteadyClock::now();

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

    const SteadyClock::time_point importStart = SteadyClock::now();
    const int32_t importResult = af_engine_import_glb(session->engine, path);
    session->importMicros = microsBetween(importStart, SteadyClock::now());
    const std::string importError = af_engine_last_error(session->engine)
        ? af_engine_last_error(session->engine) : "";
    env->ReleaseStringUTFChars(glbPath, path);
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

namespace {
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
        resolved.outfit != AF_OUTFIT_JEANS_JACKET ||
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
    return createGeneratedSession(env, surface, width, height, false);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeCreateGeneratedDebug(
    JNIEnv* env, jobject, jobject surface, jint width, jint height, jboolean debugMagenta) {
    return createGeneratedSession(env, surface, width, height, debugMagenta == JNI_TRUE);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeRenderGenerated(
    JNIEnv*, jobject, jlong handle, jfloat deltaSeconds) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    session->renderer->renderFrame(deltaSeconds);
    return session->renderer->stats().framePresented ? JNI_TRUE : JNI_FALSE;
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

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeResize(
    JNIEnv*, jobject, jlong handle, jint width, jint height) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || width <= 0 || height <= 0) return;
    session->width = width;
    session->height = height;
    session->renderer->resize(width, height);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeRender(
    JNIEnv*, jobject, jlong handle, jfloat deltaSeconds) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return JNI_FALSE;
    if (session->framesRequested++ == 0) {
        const SteadyClock::time_point firstStart = SteadyClock::now();
        session->renderer->renderFrame(deltaSeconds);
        session->firstFrameMicros = microsBetween(firstStart, SteadyClock::now());
    } else {
        session->renderer->renderFrame(deltaSeconds);
    }
    return session->renderer->stats().framePresented ? JNI_TRUE : JNI_FALSE;
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

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeDestroy(
    JNIEnv*, jobject, jlong handle) {
    delete reinterpret_cast<NativeAvatarSession*>(handle);
}

