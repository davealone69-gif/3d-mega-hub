#include "af_scene.h"

#include <cstdio>

namespace {
int failures = 0;
void check(const char* label, bool pass) {
    std::printf("%s %s\n", pass ? "PASS" : "FAIL", label);
    if (!pass) ++failures;
}
}

int main() {
    af::Scene scene;
    const int root = scene.addNode("asset");
    auto add = [&](const char* name, const af::Vec3& centre) {
        const int node = scene.addNode(name, root);
        af::Geometry geometry;
        geometry.name = name;
        geometry.vertexCount = 3;
        geometry.indexCount = 3;
        geometry.bounds.centre = centre;
        geometry.bounds.radius = 0.1f;
        geometry.lodRanges[0] = {0, 3};
        const int geometryIndex = scene.addGeometry(geometry);
        return scene.addInstance(name, geometryIndex, node);
    };

    const int visible = add("visible", af::Vec3(0.0f, 0.0f, 0.0f));
    add("outside", af::Vec3(100.0f, 0.0f, 0.0f));
    check("scene transforms resolve", scene.updateTransforms());

    af::Camera camera;
    camera.position = af::Vec3(0.0f, 0.0f, 5.0f);
    camera.target = af::Vec3(0.0f, 0.0f, 0.0f);
    camera.aspect = 1.0f;
    af::Visibility result;
    scene.buildVisibility(camera, 1.0f, &result);

    check("in-frustum primitive is submitted",
          result.draws.size() == 1 && result.draws[0].instance == visible);
    check("out-of-frustum primitive is culled", result.culledInstances == 1);
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
