/*
 * camera_frustum_test.cpp -- host regression for GLES camera fitting.
 *
 * Tests the real header-only buildCameraUbo implementation against a 1.75 m AABB.
 */
#include "af_render_common.h"
#include "avatarforge_live_rig.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
int failures = 0;

void check(const char* name, bool condition) {
    if (condition) {
        std::printf("PASS  %s\n", name);
    } else {
        ++failures;
        std::printf("FAIL  %s\n", name);
    }
}
}

int main() {
    af::FrameScene scene{};
    scene.hasModel = true;
    scene.modelMin[0] = -0.25f;
    scene.modelMin[1] = 0.0f;
    scene.modelMin[2] = -0.20f;
    scene.modelMax[0] = 0.25f;
    scene.modelMax[1] = 1.75f;
    scene.modelMax[2] = 0.20f;

    af::CameraState camera{};
    camera.targetX = 0.0f;
    camera.targetY = 0.875f;
    camera.targetZ = 0.0f;
    camera.yawDeg = 180.0f;
    camera.pitchDeg = 4.0f;
    camera.distance = 4.0f;
    camera.fovDeg = 32.0f;

    af::RendererOptions options{};
    af::EnvironmentParams environment{};
    af::ShGlobal sh{};

    af::buildCameraUbo(&scene, camera, options, environment, sh,
                       1080, 1920, false);

    check("camera zNear is finite", std::isfinite(scene.nearPlane));
    check("camera zFar is finite", std::isfinite(scene.farPlane));
    check("camera zNear is positive", scene.nearPlane > 0.0f);
    check("camera zFar exceeds zNear", scene.farPlane > scene.nearPlane);

    af::Mat4 viewProjection{};
    std::memcpy(viewProjection.m, scene.camera.viewProj, sizeof(viewProjection.m));

    int cornersInDepthRange = 0;
    int cornersWithPositiveW = 0;
    for (int corner = 0; corner < 8; ++corner) {
        const float point[4] = {
            (corner & 1) ? scene.modelMax[0] : scene.modelMin[0],
            (corner & 2) ? scene.modelMax[1] : scene.modelMin[1],
            (corner & 4) ? scene.modelMax[2] : scene.modelMin[2],
            1.0f
        };
        float clip[4] = {};
        af::matMulVec4(viewProjection, point, clip);

        const bool finiteClip = std::isfinite(clip[0]) &&
            std::isfinite(clip[1]) && std::isfinite(clip[2]) &&
            std::isfinite(clip[3]);
        check("AABB clip coordinates are finite", finiteClip);
        if (!finiteClip) continue;

        const bool positiveW = clip[3] > 0.0f;
        check("AABB clipW is positive", positiveW);
        if (positiveW) ++cornersWithPositiveW;

        check("AABB clip Z is within GLES [-W,+W]",
              clip[2] >= -clip[3] && clip[2] <= clip[3]);

        const float* view = scene.camera.view;
        const float viewZ = view[2] * point[0] + view[6] * point[1] +
                            view[10] * point[2] + view[14];
        const float viewDepth = -viewZ;
        const bool depthValid = std::isfinite(viewDepth) &&
            viewDepth >= scene.nearPlane && viewDepth <= scene.farPlane;
        check("AABB view depth is between zNear and zFar", depthValid);
        if (positiveW && depthValid) ++cornersInDepthRange;
    }

    check("all eight AABB corners have positive clipW", cornersWithPositiveW == 8);
    check("all eight AABB corners pass camera depth range", cornersInDepthRange == 8);

    // Regression: origin-centered foreign GLBs (e.g. Y in [-0.5, +0.5]) must anchor ground at modelMin[1].
    scene.modelMin[1] = -0.4999f;
    scene.modelMax[1] =  0.4999f;
    af::buildCameraUbo(&scene, camera, options, environment, sh, 1080, 1920, false);
    check("ground plane Y tracks negative modelMin[1]",
          std::fabs(scene.camera.ground[0] - (-0.4999f)) < 1e-5f);

    // Regression: LiveRigRegistry handoff, clip+ARKit blendshape composition, and tap-to-inspect pickRegion.
    {
        af_spec spec{};
        af_spec_init(&spec);
        spec.seed = 1337;
        spec.sex = AF_SEX_FEMALE;
        spec.build = AF_BUILD_ATHLETIC;
        spec.hair_style = AF_HAIR_MEDIUM;
        spec.outfit = AF_OUTFIT_TSHIRT;

        af_engine* rawEngine = nullptr;
        check("af_engine_create for live rig test", af_engine_create(&spec, nullptr, &rawEngine) == AF_OK && rawEngine != nullptr);
        if (rawEngine) {
            check("af_engine_run_all succeeds for live rig", af_engine_run_all(rawEngine) == AF_OK);
            af_engine_clear_callbacks(rawEngine);
            af::LiveRigRegistry::stageGenerated(std::shared_ptr<af_engine>(rawEngine, &af_engine_destroy));
            af::LiveRigRegistry::bindPublishedPath("/tmp/live_rig_test.glb");

            auto acquired = af::LiveRigRegistry::acquireForPath("/tmp/live_rig_test.glb");
            check("LiveRigRegistry acquires published engine", acquired != nullptr);
            check("LiveRigRegistry rejects non-matching path",
                  af::LiveRigRegistry::acquireForPath("/tmp/other.glb") == nullptr);

            if (acquired) {
                check("live rig retains 71 joints", af_engine_joint_count(acquired.get()) == 71);
                af_stats stats{};
                check("live rig stats report 52 ARKit blendshapes",
                      af_engine_stats(acquired.get(), &stats) == AF_OK && stats.blendshape_count == 52);

                check("live rig applies AF_CLIP_WALK", af_engine_apply_clip(acquired.get(), AF_CLIP_WALK, 0.35f) == AF_OK);
                float exprWeights[52] = {};
                exprWeights[8] = 1.0f;   // eyeBlinkLeft
                exprWeights[9] = 1.0f;   // eyeBlinkRight
                exprWeights[43] = 0.85f; // mouthSmileLeft
                exprWeights[44] = 0.85f; // mouthSmileRight
                check("live rig composes ARKit blendshapes with clip",
                      af_engine_apply_blendshapes(acquired.get(), exprWeights, 52) == AF_OK);

                check("live rig resets to AF_CLIP_REST", af_engine_apply_clip(acquired.get(), AF_CLIP_REST, 0.0f) == AF_OK);
                const float frontOrig[3] = {0.0f, 1.15f, -2.2f};
                const float frontDir[3] = {0.0f, 0.0f, 1.0f};
                float hitDist = 0.0f;
                const int hitRegion = af::pickRegion(acquired.get(), frontOrig, frontDir, &hitDist, nullptr);
                check("pickRegion hits live avatar torso/chest from front camera",
                      hitRegion >= 0 && hitDist > 0.5f && hitDist < 4.0f);

                const float missOrig[3] = {5.0f, 1.15f, -2.2f};
                float missDist = 0.0f;
                const int missRegion = af::pickRegion(acquired.get(), missOrig, frontDir, &missDist, nullptr);
                check("pickRegion returns -1 on off-target background ray", missRegion == -1);
            }
            af::LiveRigRegistry::clear();
            check("LiveRigRegistry clear releases cached engine",
                  af::LiveRigRegistry::acquireForPath("/tmp/live_rig_test.glb") == nullptr);
        }
    }

    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

