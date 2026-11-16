#include "af_avatar_lod.h"
#include "af_mesh.h"   /* af::REGION_SKIN, af::REGION_HAIR */

#include <cstdio>
#include <vector>

namespace {
int failures = 0;
void check(const char* label, bool ok) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}
}

int main() {
    constexpr int side = 24;
    std::vector<af_vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(side * side);
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            af_vertex v{};
            v.pos[0] = float(x) / float(side - 1);
            v.pos[1] = float(y) / float(side - 1);
            v.pos[2] = 0.0f;
            v.normal[2] = 1.0f;
            v.weights[0] = 1.0f;
            vertices.push_back(v);
        }
    }
    for (int y = 0; y < side - 1; ++y) {
        for (int x = 0; x < side - 1; ++x) {
            const uint32_t a = uint32_t(y * side + x);
            const uint32_t b = a + 1;
            const uint32_t c = a + uint32_t(side);
            const uint32_t d = c + 1;
            indices.insert(indices.end(), {a, b, c, b, d, c});
        }
    }
    af_submesh submesh{};
    submesh.vertex_offset = 0;
    submesh.vertex_count = static_cast<int32_t>(vertices.size());
    submesh.index_offset = 0;
    submesh.index_count = static_cast<int32_t>(indices.size());

    af::AvatarLodSet lod;
    check("valid indexed mesh builds", lod.build(vertices.data(), vertices.size(),
          indices.data(), indices.size(), &submesh, 1));
    check("LOD0 exactly preserves source index count",
          lod.levelCount() >= 1 && lod.level(0).indexCount == indices.size());
    check("coarser LOD is generated when topology permits",
          lod.levelCount() >= 2 && lod.level(1).indexCount < indices.size());
    bool valid = true;
    for (uint32_t index : lod.indices()) valid = valid && index < vertices.size();
    check("all LOD indices reference original vertices", valid);
    bool ranges = true;
    for (int level = 0; level < lod.levelCount(); ++level) {
        const auto& item = lod.level(level);
        ranges = ranges && item.firstIndex + item.indexCount <= lod.indices().size();
        ranges = ranges && item.submeshes.size() == 1;
        if (!item.submeshes.empty()) {
            const auto& range = item.submeshes[0];
            ranges = ranges && range.firstIndex >= item.firstIndex;
            ranges = ranges && range.firstIndex + range.indexCount <= item.firstIndex + item.indexCount;
        }
    }
    check("each LOD has bounded submesh index ranges", ranges);

    /*
     * Region-aware LOD: hair must survive simplification far better than skin, because the
     * silhouette, braid strands and clump channels are the first thing clustering destroys.
     * Two identical 24x24 grids give a like-for-like retention ratio.
     */
    std::vector<af_vertex> regionVertices(vertices);
    for (auto& vertex : regionVertices) vertex.pos[2] = 0.0f;
    std::vector<af_vertex> hairVertices(vertices);
    for (auto& vertex : hairVertices) {
        vertex.pos[0] += 2.0f;   /* separate space so the two grids never share clusters */
        vertex.pos[2] = 1.0f;
    }
    regionVertices.insert(regionVertices.end(), hairVertices.begin(), hairVertices.end());
    const auto gridIndices = indices;
    std::vector<uint32_t> regionIndices(gridIndices);
    for (uint32_t index : gridIndices) {
        regionIndices.push_back(index + static_cast<uint32_t>(vertices.size()));
    }

    af_submesh regions[2]{};
    regions[0].region = af::REGION_SKIN;
    regions[0].vertex_offset = 0;
    regions[0].vertex_count = static_cast<int32_t>(vertices.size());
    regions[0].index_offset = 0;
    regions[0].index_count = static_cast<int32_t>(gridIndices.size());
    regions[1].region = af::REGION_HAIR;
    regions[1].vertex_offset = static_cast<int32_t>(vertices.size());
    regions[1].vertex_count = static_cast<int32_t>(hairVertices.size());
    regions[1].index_offset = static_cast<int32_t>(gridIndices.size());
    regions[1].index_count = static_cast<int32_t>(gridIndices.size());

    af::AvatarLodSet regionalLod;
    check("body+hair mesh builds three levels",
          regionalLod.build(regionVertices.data(), regionVertices.size(), regionIndices.data(),
                            regionIndices.size(), regions, 2) &&
              regionalLod.levelCount() == af::kAvatarLodLevels);
    if (regionalLod.levelCount() >= 2) {
        const auto& coarse = regionalLod.level(1);
        const float bodyRetention = static_cast<float>(coarse.submeshes[0].indexCount) /
                                    static_cast<float>(gridIndices.size());
        const float hairRetention = static_cast<float>(coarse.submeshes[1].indexCount) /
                                    static_cast<float>(gridIndices.size());
        check("hair keeps more triangles than skin at LOD1", hairRetention > bodyRetention);
        check("hair keeps at least a third of its triangles at LOD1", hairRetention >= 0.33f);
        check("skin is still simplified at LOD1", bodyRetention < 0.5f);
        bool bounded = true;
        for (const auto& range : coarse.submeshes) {
            bounded = bounded && range.firstIndex + range.indexCount <=
                                      coarse.firstIndex + coarse.indexCount;
        }
        check("region-aware LOD ranges stay inside the level", bounded);
    }
    af::AvatarLodSet regionalLodAgain;
    regionalLodAgain.build(regionVertices.data(), regionVertices.size(), regionIndices.data(),
                          regionIndices.size(), regions, 2);
    check("region-aware LOD is deterministic",
          regionalLodAgain.levelCount() == regionalLod.levelCount() &&
              regionalLodAgain.indices() == regionalLod.indices());

    af::AvatarLodSet rejected;
    check("malformed index count is rejected",
          !rejected.build(vertices.data(), vertices.size(), indices.data(), indices.size() - 1,
                          &submesh, 1));
    check("failed build leaves no partial levels", rejected.levelCount() == 0);
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
