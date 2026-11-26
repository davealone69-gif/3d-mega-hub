/*
 * af_avatar_lod.h -- renderer-owned, deformation-safe index-only avatar LODs.
 *
 * LOD levels contain indices into the engine's original interleaved vertex stream. No vertex is
 * averaged, reweighted, re-morphed, or reordered: animation and blendshape updates can therefore
 * keep uploading the kernel's live vertex buffer unchanged while the renderer switches index
 * ranges. Simplification is chart-, submesh-, and skin-influence-local; unsupported input falls
 * back to the exact source indices.
 *
 * The set also measures each level's rest-pose surface area, which is what lets the renderer pick
 * a level from the size its triangles actually land on the screen (af_lod_select.h) instead of from
 * a camera distance in metres.
 */
#ifndef AF_AVATAR_LOD_H
#define AF_AVATAR_LOD_H

#include "afcore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace af {

constexpr int kAvatarLodLevels = 3;

struct AvatarLodRange {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

struct AvatarLodLevel {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    std::vector<AvatarLodRange> submeshes;
};

class AvatarLodSet {
public:
    /*
     * Builds LOD 0 (the exact source indices) and up to two progressively coarser index streams.
     * Returns false only when the input arrays/ranges are malformed; on failure the set is empty.
     * LOD indices always refer to original vertex indices, preserving the engine's CPU deformation
     * and every source vertex attribute. `restVertices` provides stable clustering bounds; the
     * renderer still draws the current/deformed vertices.
     */
    bool build(const af_vertex* restVertices, size_t vertexCount,
               const uint32_t* sourceIndices, size_t indexCount,
               const af_submesh* submeshes, size_t submeshCount);

    void clear();
    int levelCount() const { return static_cast<int>(levels_.size()); }
    const AvatarLodLevel& level(int index) const;
    const std::vector<uint32_t>& indices() const { return indices_; }
    int triangleCount(int level) const;

    /*
     * Rest-pose surface area in square metres, MEASURED by summing the triangles of that level,
     * and the mean triangle area that follows from it. Both are computed once per build from the
     * rest vertices, not estimated from a bounding box and not recomputed per frame.
     *
     * The screen-space LOD selection in af_lod_select.h needs the mean triangle area of each level
     * to work out how large that level's triangles land on the viewport. Returning 0 means "this
     * level has no measured area" (an empty level, or a build that failed), and callers treat 0 as
     * unmeasurable rather than as free.
     */
    float surfaceAreaM2(int level) const;
    float meanTriangleAreaM2(int level) const;
    bool  areasValid() const { return areasValid_; }

private:
    std::vector<uint32_t> indices_;
    std::vector<AvatarLodLevel> levels_;
    std::vector<float> levelAreaM2_;   /* per level, measured from the rest pose; parallels levels_ */
    bool areasValid_ = false;
};

} /* namespace af */

#endif /* AF_AVATAR_LOD_H */
