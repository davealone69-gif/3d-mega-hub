/*
 * camera_frustum_test.cpp -- host regression for GLES camera fitting.
 *
 * Tests the real header-only buildCameraUbo implementation against a 1.75 m AABB.
 */
#include "af_render_common.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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

    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

