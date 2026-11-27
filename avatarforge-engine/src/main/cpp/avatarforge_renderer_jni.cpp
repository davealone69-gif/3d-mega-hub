#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "afcore.h"
#include "avatarforge_live_rig.h"
#include "render/af_render.h"
#include "render/af_render_common.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace {
struct NativeAvatarSession {
    ANativeWindow* window = nullptr;
    std::shared_ptr<af_engine> liveEngine;
    af_engine* engine = nullptr;
    af::Renderer* renderer = nullptr;
    int width = 0;
    int height = 0;
    af::CameraState camera{};
    af::CameraState initialCamera{};
    af::RendererOptions baseOptions{};
    bool allForeignSubmeshes = false;
    bool isLiveRig = false;
    int32_t highlightedRegion = -1;
    float boundsMin[3] = {0.0f, 0.0f, 0.0f};
    float boundsMax[3] = {0.0f, 1.75f, 0.0f};
    float minDistance = 0.05f;
    float maxDistance = 100.0f;
    std::string verificationReport;
    ~NativeAvatarSession() {
        // Renderer may still reference both the engine buffers and the native window.
        if (renderer) {
            renderer->waitIdle();
            renderer->attachEngine(nullptr);
            delete renderer;
            renderer = nullptr;
        }
        if (liveEngine) {
            engine = nullptr;
            liveEngine.reset();
        } else if (engine) {
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
    JNIEnv* env, jobject, jobject surface, jstring glbPath, jint width, jint height,
    jboolean preferVulkan, jboolean allowLiveRig) {
    if (!surface || !glbPath || width <= 0 || height <= 0) {
        throwState(env, "AvatarForge renderer requires a Surface, GLB path and non-zero size");
        return 0;
    }
    const char* path = env->GetStringUTFChars(glbPath, nullptr);
    if (!path) return 0;
    const std::string pathStr(path);
    env->ReleaseStringUTFChars(glbPath, path);

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

    if (allowLiveRig == JNI_TRUE) {
        auto live = af::LiveRigRegistry::acquireForPath(pathStr);
        if (live && af_engine_submesh_count(live.get()) > 0) {
            session->liveEngine = std::move(live);
            session->engine = session->liveEngine.get();
            session->isLiveRig = true;
            float zeroWeights[52] = {};
            af_engine_apply_blendshapes(session->engine, zeroWeights, 52);
        }
    }

    if (!session->engine) {
        af_spec spec{};
        af_spec_init(&spec);
        const int32_t createResult = af_engine_create(&spec, nullptr, &session->engine);
        if (createResult != AF_OK || !session->engine) {
            throwState(env, "af_engine_create failed: " + std::to_string(createResult));
            return 0;
        }

        const int32_t importResult = af_engine_import_glb(session->engine, pathStr.c_str());
        const std::string importError = af_engine_last_error(session->engine)
            ? af_engine_last_error(session->engine) : "";
        if (importResult != AF_OK) {
            throwState(env, "AvatarForge GLB import failed (" + std::to_string(importResult) +
                            "): " + importError);
            return 0;
        }
    }

    /* Apply the initial skin pose before reading submesh bounds for camera framing.
       Static GLBs have no joints and keep their original import path. */
    if (af_engine_joint_count(session->engine) > 0) {
        const int32_t poseResult = af_engine_apply_clip(session->engine, AF_CLIP_REST, 0.0f);
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
    /* Vulkan-first on hardware Android GPUs (Xclipse 940, Adreno, Mali, Immortalis) with
       automatic GLES 3.1 fallback on software emulators or when Vulkan is unavailable. */
    options.preferVulkan = (preferVulkan == JNI_TRUE);
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
    session->renderer = af::Renderer::create(session->window, options, &rendererError);
    if (!session->renderer) {
        throwState(env, "AvatarForge renderer creation failed: " + rendererError);
        return 0;
    }
    if (!session->renderer->resize(width, height)) {
        throwState(env, "AvatarForge renderer rejected the Android surface dimensions");
        return 0;
    }
    session->renderer->attachEngine(session->engine);
    session->renderer->invalidateMesh();

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
    session->baseOptions = options;
    session->allForeignSubmeshes = allForeignSubmeshes;
    for (int axis = 0; axis < 3; ++axis) {
        session->boundsMin[axis] = boundsMin[axis];
        session->boundsMax[axis] = boundsMax[axis];
    }
    session->camera = camera;
    session->initialCamera = camera;
    session->minDistance = std::max(0.02f, radius * 0.25f);
    session->maxDistance = std::max(session->minDistance * 8.0f, camera.distance * 6.0f);
    // buildCameraUbo derives near/far from the imported model bounds each frame.
    // Keep clipping-plane policy in one place instead of maintaining a second camera contract.
    session->renderer->setCamera(camera);
    return reinterpret_cast<jlong>(session.release());
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
    return reinterpret_cast<jlong>(session.release());
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
    session->renderer->renderFrame(deltaSeconds);
    return session->renderer->stats().framePresented ? JNI_TRUE : JNI_FALSE;
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
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetCameraPreset(
    JNIEnv*, jobject, jlong handle, jint presetId) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    af::CameraState cam = session->initialCamera;
    const float baseYaw = session->initialCamera.yawDeg;
    const float dy = std::max(0.05f, session->boundsMax[1] - session->boundsMin[1]);
    const float dz = std::max(0.05f, session->boundsMax[2] - session->boundsMin[2]);
    switch (presetId) {
        case 1: { // Portrait / Face close-up (frames dual-mesh eyes, 3D brows/lashes, hair cards)
            cam.targetY = session->boundsMin[1] + dy * 0.875f;
            cam.yawDeg = std::fmod(baseYaw - 12.0f + 360.0f, 360.0f);
            cam.pitchDeg = 2.0f;
            cam.fovDeg = 26.0f;
            cam.distance = std::max(session->minDistance, std::max(dy * 0.36f, dz * 1.45f));
            break;
        }
        case 2: { // 3/4 Hero
            cam.yawDeg = std::fmod(baseYaw - 34.0f + 360.0f, 360.0f);
            cam.pitchDeg = 6.0f;
            cam.distance = std::max(session->minDistance, session->initialCamera.distance * 0.88f);
            break;
        }
        case 3: { // Side Profile
            cam.yawDeg = std::fmod(baseYaw + 90.0f, 360.0f);
            cam.pitchDeg = 3.0f;
            cam.distance = std::max(session->minDistance, session->initialCamera.distance * 0.92f);
            break;
        }
        case 4: { // Back View
            cam.yawDeg = std::fmod(baseYaw + 180.0f, 360.0f);
            cam.pitchDeg = 4.0f;
            cam.distance = std::max(session->minDistance, session->initialCamera.distance * 0.96f);
            break;
        }
        default: // 0 = Full Body
            break;
    }
    cam.distance = std::max(session->minDistance, std::min(session->maxDistance, cam.distance));
    session->camera = cam;
    session->renderer->setCamera(session->camera);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetLightingPreset(
    JNIEnv*, jobject, jlong handle, jint presetId) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    af::RendererOptions opt = session->baseOptions;
    const float baseAz = session->baseOptions.sunAzimuthDeg;
    switch (presetId) {
        case 1: { // Golden Hour
            opt.sunAzimuthDeg = std::fmod(baseAz + 32.0f, 360.0f);
            opt.sunElevationDeg = 12.0f;
            opt.sunIntensity = 1.85f;
            opt.turbidity = 4.2f;
            opt.environmentIntensity = 0.72f;
            opt.exposure = 0.88f;
            opt.enableBloom = true;
            break;
        }
        case 2: { // Dramatic Rim
            opt.sunAzimuthDeg = std::fmod(baseAz + 138.0f, 360.0f);
            opt.sunElevationDeg = 22.0f;
            opt.sunIntensity = 2.10f;
            opt.turbidity = 2.2f;
            opt.environmentIntensity = 0.36f;
            opt.exposure = 0.78f;
            opt.enableBloom = true;
            break;
        }
        case 3: { // Soft Overcast
            opt.sunAzimuthDeg = baseAz;
            opt.sunElevationDeg = 62.0f;
            opt.sunIntensity = 0.78f;
            opt.turbidity = 5.2f;
            opt.environmentIntensity = 1.15f;
            opt.exposure = 0.86f;
            opt.enableBloom = false;
            break;
        }
        default: // 0 = Studio
            break;
    }
    session->renderer->setOptions(opt);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetInspectionModes(
    JNIEnv*, jobject, jlong handle, jboolean showSkeleton, jboolean flatShading) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    session->renderer->setSkeletonVisible(showSkeleton == JNI_TRUE);
    session->renderer->setFlatShading(flatShading == JNI_TRUE);
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeGetCapabilities(
    JNIEnv* env, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    jint values[3] = {0, 0, 0};
    if (session && session->engine) {
        values[0] = static_cast<jint>(af_engine_joint_count(session->engine));
        af_stats stats{};
        if (af_engine_stats(session->engine, &stats) == AF_OK) {
            values[1] = static_cast<jint>(stats.blendshape_count);
        }
        values[2] = session->isLiveRig ? 1 : 0;
    }
    jintArray out = env->NewIntArray(3);
    if (out) env->SetIntArrayRegion(out, 0, 3, values);
    return out;
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeSetAnimationClip(
    JNIEnv*, jobject, jlong handle, jint clipId, jfloat speed, jboolean loop) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || !session->engine) return;
    if (af_engine_joint_count(session->engine) <= 0) return;
    const int32_t safeClip = std::clamp<int32_t>(clipId, AF_CLIP_REST, AF_CLIP_COUNT - 1);
    const float safeSpeed = std::isfinite(speed) ? std::clamp(speed, 0.1f, 3.0f) : 1.0f;
    session->renderer->setClip(safeClip, safeSpeed, loop == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeApplyExpression(
    JNIEnv*, jobject, jlong handle, jint expressionId) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer || !session->engine) return;
    if (af_engine_joint_count(session->engine) <= 0) return;
    // Matches kBlendshapeNames[52] in af_rig.cpp.
    float w[52] = {};
    switch (expressionId) {
        case 1: // Smile
            w[43] = 0.85f; // mouthSmileLeft
            w[44] = 0.85f; // mouthSmileRight
            w[6]  = 0.45f; // cheekSquintLeft
            w[7]  = 0.45f; // cheekSquintRight
            w[18] = 0.25f; // eyeSquintLeft
            w[19] = 0.25f; // eyeSquintRight
            w[27] = 0.30f; // mouthDimpleLeft
            w[28] = 0.30f; // mouthDimpleRight
            break;
        case 2: // Blink
            w[8]  = 1.00f; // eyeBlinkLeft
            w[9]  = 1.00f; // eyeBlinkRight
            break;
        case 3: // Wink
            w[8]  = 1.00f; // eyeBlinkLeft
            w[6]  = 0.40f; // cheekSquintLeft
            w[43] = 0.65f; // mouthSmileLeft
            w[44] = 0.35f; // mouthSmileRight
            w[4]  = 0.35f; // browOuterUpRight
            break;
        case 4: // Surprised
            w[2]  = 0.85f; // browInnerUp
            w[3]  = 0.80f; // browOuterUpLeft
            w[4]  = 0.80f; // browOuterUpRight
            w[20] = 0.90f; // eyeWideLeft
            w[21] = 0.90f; // eyeWideRight
            w[24] = 0.72f; // jawOpen
            break;
        case 5: // Frown
            w[0]  = 0.80f; // browDownLeft
            w[1]  = 0.80f; // browDownRight
            w[29] = 0.75f; // mouthFrownLeft
            w[30] = 0.75f; // mouthFrownRight
            w[49] = 0.25f; // noseSneerLeft
            w[50] = 0.25f; // noseSneerRight
            break;
        case 6: // Gaze Left (character's left: left eye out, right eye in)
            w[14] = 0.90f; // eyeLookOutLeft
            w[13] = 0.90f; // eyeLookInRight
            break;
        case 7: // Gaze Right (character's right: left eye in, right eye out)
            w[12] = 0.90f; // eyeLookInLeft
            w[15] = 0.90f; // eyeLookOutRight
            break;
        default: // 0 = Neutral
            break;
    }
    if (af_engine_apply_blendshapes(session->engine, w, 52) == AF_OK) {
        session->renderer->invalidateMesh();
    }
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativePickRegion(
    JNIEnv* env, jobject, jlong handle, jfloat normX, jfloat normY) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    jfloat result[2] = {-1.0f, 0.0f};
    if (session && session->renderer && session->engine && session->width > 0 && session->height > 0 &&
        std::isfinite(normX) && std::isfinite(normY)) {
        const af::CameraState& cam = session->camera;
        const float aspect = static_cast<float>(session->width) / static_cast<float>(session->height);
        const float yaw = cam.yawDeg * 3.14159265358979323846f / 180.0f;
        const float pitch = cam.pitchDeg * 3.14159265358979323846f / 180.0f;
        const float dist = cam.distance < 0.3f ? 0.3f : cam.distance;
        const af::Vec3 target{cam.targetX, cam.targetY, cam.targetZ};
        const af::Vec3 eye{
            cam.targetX + dist * std::cos(pitch) * std::sin(yaw),
            cam.targetY + dist * std::sin(pitch),
            cam.targetZ + dist * std::cos(pitch) * std::cos(yaw)
        };
        const af::Vec3 forward = af::vnormalize(af::vsub(target, eye));
        const af::Vec3 worldUp{0.0f, 1.0f, 0.0f};
        af::Vec3 right = af::vcross(forward, worldUp);
        if (af::vdot(right, right) < 1e-10f) right = af::Vec3{1.0f, 0.0f, 0.0f};
        else right = af::vnormalize(right);
        const af::Vec3 camUp = af::vcross(right, forward);
        const float safeFov = std::isfinite(cam.fovDeg) ? std::clamp(cam.fovDeg, 10.0f, 120.0f) : 32.0f;
        const float tanHalfV = std::tan(safeFov * 3.14159265358979323846f / 360.0f);
        const float ndcX = std::clamp(normX, 0.0f, 1.0f) * 2.0f - 1.0f;
        const float ndcY = 1.0f - std::clamp(normY, 0.0f, 1.0f) * 2.0f;
        const af::Vec3 rayDir = af::vnormalize(
            af::vadd(af::vadd(forward, af::vscale(right, ndcX * aspect * tanHalfV)),
                     af::vscale(camUp, ndcY * tanHalfV)));

        const float origArr[3] = {eye.x, eye.y, eye.z};
        const float dirArr[3] = {rayDir.x, rayDir.y, rayDir.z};
        float hitDist = 0.0f;
        const int32_t hitRegion = af::pickRegion(session->engine, origArr, dirArr, &hitDist, nullptr);
        if (hitRegion >= 0 && hitRegion == session->highlightedRegion) {
            session->highlightedRegion = -1;
            hitDist = 0.0f;
        } else {
            session->highlightedRegion = hitRegion;
        }
        session->renderer->setHighlightRegion(session->highlightedRegion);
        result[0] = static_cast<jfloat>(session->highlightedRegion);
        result[1] = session->highlightedRegion >= 0 ? hitDist : 0.0f;
    }
    jfloatArray out = env->NewFloatArray(2);
    if (out) env->SetFloatArrayRegion(out, 0, 2, result);
    return out;
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeClearHighlight(
    JNIEnv*, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (!session || !session->renderer) return;
    session->highlightedRegion = -1;
    session->renderer->setHighlightRegion(-1);
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeWaitIdle(
    JNIEnv*, jobject, jlong handle) {
    auto* session = reinterpret_cast<NativeAvatarSession*>(handle);
    if (session && session->renderer) session->renderer->waitIdle();
}

extern "C" JNIEXPORT void JNICALL
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeDestroy(
    JNIEnv*, jobject, jlong handle) {
    delete reinterpret_cast<NativeAvatarSession*>(handle);
}

