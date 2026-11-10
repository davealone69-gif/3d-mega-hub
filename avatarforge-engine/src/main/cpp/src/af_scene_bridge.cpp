#include "af_scene_bridge.h"
#include "af_scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

struct af_scene_bridge {
    const af_engine* engine = nullptr;
    int32_t submeshCount = -1;
    af::Scene scene;
    af::Visibility visibility;
};

extern "C" {

af_scene_bridge* af_scene_bridge_create(void) {
    return new (std::nothrow) af_scene_bridge();
}

void af_scene_bridge_destroy(af_scene_bridge* bridge) {
    delete bridge;
}

int32_t af_scene_bridge_update(af_scene_bridge* bridge, const af_engine* engine,
                               const float view_projection[16], const float eye[3],
                               float quality, af_scene_draw* out_draws, int32_t capacity,
                               int32_t* out_count, int32_t* out_culled) {
    if (!bridge || !engine || !view_projection || !eye || !out_count || !out_culled || capacity < 0) {
        return AF_ERR_INVALID_ARG;
    }
    *out_count = 0;
    *out_culled = 0;

    const int32_t count = af_engine_submesh_count(engine);
    const af_submesh* submeshes = af_engine_submeshes(engine);
    if (count < 0 || (count > 0 && !submeshes) || count > capacity ||
        (count > 0 && !out_draws)) {
        return AF_ERR_INVALID_ARG;
    }
    if (count == 0) {
        bridge->scene.clear();
        bridge->visibility.clear();
        bridge->engine = engine;
        bridge->submeshCount = 0;
        return AF_OK;
    }

    if (bridge->engine != engine || bridge->submeshCount != count ||
        bridge->scene.instanceCount() != static_cast<size_t>(count)) {
        bridge->scene.clear();
        const int root = bridge->scene.addNode("Imported asset");
        for (int i = 0; i < count; ++i) {
            const std::string name = "Submesh " + std::to_string(i);
            const int node = bridge->scene.addNode(name, root);
            af::Geometry geometry;
            geometry.name = name;
            const int geometryIndex = bridge->scene.addGeometry(std::move(geometry));
            bridge->scene.addInstance(name, geometryIndex, node);
        }
        bridge->engine = engine;
        bridge->submeshCount = count;
    }

    for (int i = 0; i < count; ++i) {
        const af_submesh& submesh = submeshes[i];
        if (submesh.index_offset < 0 || submesh.index_count < 0 ||
            submesh.vertex_offset < 0 || submesh.vertex_count < 0) {
            return AF_ERR_INVALID_ARG;
        }
        const float minX = submesh.bounds_min[0], minY = submesh.bounds_min[1], minZ = submesh.bounds_min[2];
        const float maxX = submesh.bounds_max[0], maxY = submesh.bounds_max[1], maxZ = submesh.bounds_max[2];
        if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(minZ) ||
            !std::isfinite(maxX) || !std::isfinite(maxY) || !std::isfinite(maxZ) ||
            minX > maxX || minY > maxY || minZ > maxZ) {
            return AF_ERR_INVALID_ARG;
        }

        af::Geometry& geometry = bridge->scene.geometry(static_cast<size_t>(i));
        geometry.vertexCount = static_cast<size_t>(submesh.vertex_count);
        geometry.indexCount = static_cast<size_t>(submesh.index_count);
        geometry.bounds.centre = af::Vec3((minX + maxX) * 0.5f,
                                          (minY + maxY) * 0.5f,
                                          (minZ + maxZ) * 0.5f);
        const float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
        geometry.bounds.radius = std::max(0.0001f, 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz));
        geometry.lodCount = 1;
        geometry.lodRanges[0].indexOffset = static_cast<size_t>(submesh.index_offset);
        geometry.lodRanges[0].indexCount = static_cast<size_t>(submesh.index_count);
        af::SceneInstance& instance = bridge->scene.instance(static_cast<size_t>(i));
        instance.localBounds = geometry.bounds;
        instance.lodCount = 1;
        instance.visible = true;
    }

    if (!bridge->scene.updateTransforms()) return AF_ERR_INVALID_ARG;

    af::Mat4 viewProjection{};
    std::memcpy(viewProjection.m, view_projection, sizeof(viewProjection.m));
    bridge->scene.buildVisibility(viewProjection, af::Vec3(eye[0], eye[1], eye[2]),
                                  quality, &bridge->visibility);
    if (bridge->visibility.draws.size() > static_cast<size_t>(capacity)) return AF_ERR_INVALID_ARG;

    for (size_t i = 0; i < bridge->visibility.draws.size(); ++i) {
        const af::DrawItem& item = bridge->visibility.draws[i];
        out_draws[i].instance = item.instance;
        out_draws[i].index_offset = static_cast<uint64_t>(item.indexOffset);
        out_draws[i].index_count = static_cast<uint64_t>(item.indexCount);
        out_draws[i].sort_depth = item.sortDepth;
    }
    *out_count = static_cast<int32_t>(bridge->visibility.draws.size());
    *out_culled = bridge->visibility.culledInstances;
    return AF_OK;
}

} // extern "C"
