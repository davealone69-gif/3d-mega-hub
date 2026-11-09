#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "afcore.h"
#include "render/af_render.h"

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
    af_engine* engine = nullptr;
    af::Renderer* renderer = nullptr;
    int width = 0;
    int height = 0;
    std::string verificationReport;
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

    // Fit the imported asset, not a hard-coded 1.75 m avatar. This handles tiny props,
    // large scenes and GLBs whose origin is far from their actual geometry.
    const int32_t submeshCount = af_engine_submesh_count(session->engine);
    const af_submesh* submeshes = af_engine_submeshes(session->engine);
    if (submeshCount <= 0 || !submeshes) {
        throwState(env, "AvatarForge imported no renderable submeshes");
        return 0;
    }
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
    const float halfVertical = 32.0f * 3.14159265358979323846f / 360.0f;
    const float halfHorizontal = std::atan(std::tan(halfVertical) * aspect);
    const float limitingHalfAngle = std::max(0.01f, std::min(halfVertical, halfHorizontal));
    af::CameraState camera{};
    camera.targetX = cx;
    camera.targetY = cy;
    camera.targetZ = cz;
    camera.fovDeg = 32.0f;
    camera.distance = radius / std::sin(limitingHalfAngle) * 1.15f;
    // buildCameraUbo derives near/far from the imported model bounds each frame.
    // Keep clipping-plane policy in one place instead of maintaining a second camera contract.
    session->renderer->setCamera(camera);
    return reinterpret_cast<jlong>(session.release());
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_davemega_hub_avatarforge_GeneratedAvatarRuntimeTest_nativeCreateGenerated(
    JNIEnv* env, jobject, jobject surface, jint width, jint height) {
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
    options.quality = af::QualityPreset::MEDIUM;
    options.msaaSamples = 2;
    options.enableBloom = false;
    options.groundPlane = false; // Pixel threshold must measure the avatar, not a studio floor.
    options.verifyFramePixels = true;
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
        " " + meshReport;
    return reinterpret_cast<jlong>(session.release());
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
Java_com_davemega_hub_avatarforge_AvatarForgeSurfaceView_nativeDestroy(
    JNIEnv*, jobject, jlong handle) {
    delete reinterpret_cast<NativeAvatarSession*>(handle);
}
