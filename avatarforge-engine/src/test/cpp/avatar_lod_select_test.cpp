/*
 * avatar_lod_select_test.cpp -- host regression for screen-space avatar LOD selection.
 *
 * Covers the two pieces the render path added so the colour pass stops drawing a 1.1 M-triangle
 * avatar at arm's length:
 *   1. AvatarLodSet now MEASURES each level's rest-pose surface area (af_avatar_lod.cpp), and
 *   2. af_lod_select.h turns that area plus the live camera into a level choice.
 *
 * The scenario numbers are the recorded Galaxy S24 full-body view from
 * docs/PHONE_TEST_2026-11-15.md (1080x2340, fovDeg 32, 3.0 m, ~1.1 M triangles, ~1.8 m^2 surface),
 * so the assertions check the fix against the one real device measurement the project has rather
 * than against invented ones.
 */
#include "af_lod_select.h"
#include "af_avatar_lod.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int failures = 0;

void check(const char* label, bool ok) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}

void checkNear(const char* label, float got, float want, float tol) {
    const bool ok = std::isfinite(got) && std::fabs(got - want) <= tol;
    std::printf("%s  %s (got %.5f want %.5f +/-%.5f)\n", ok ? "PASS" : "FAIL", label, got, want, tol);
    if (!ok) ++failures;
}

/* The recorded S24 view. meanTriArea per level comes from a ~1.8 m^2 surface at the level's
   triangle count: LOD0 1.1 M, LOD1 ~132k (12%), LOD2 ~55k (5%). */
af::AvatarLodScreenMetrics phoneMetrics(int level) {
    const double surface = 1.8;
    const double triangles[3] = {1100000.0, 132000.0, 55000.0};
    af::AvatarLodScreenMetrics m;
    m.viewportHeightPx = 2340.0f;
    m.fovDeg = 32.0f;
    m.viewDistanceM = 3.0f;
    m.meanTriAreaM2 = static_cast<float>(surface / triangles[level]);
    return m;
}

/* A flat unit square of two triangles in the z=0 plane: exact area 1.0 m^2. */
std::vector<af_vertex> unitSquareVerts() {
    std::vector<af_vertex> v(4);
    const float corners[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    for (int i = 0; i < 4; ++i) {
        v[i] = af_vertex{};
        for (int k = 0; k < 3; ++k) v[i].pos[k] = corners[i][k];
        v[i].normal[2] = 1.0f;
        v[i].weights[0] = 1.0f;
    }
    return v;
}

/* An n x n grid over the unit square: (n-1)^2 * 2 triangles, area 1.0 m^2, dense enough that the
   clustering in af_avatar_lod.cpp produces coarser levels to measure. */
void buildGrid(int side, std::vector<af_vertex>* verts, std::vector<uint32_t>* indices) {
    verts->clear();
    indices->clear();
    verts->reserve(static_cast<size_t>(side) * side);
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            af_vertex v{};
            v.pos[0] = static_cast<float>(x) / static_cast<float>(side - 1);
            v.pos[1] = static_cast<float>(y) / static_cast<float>(side - 1);
            v.pos[2] = 0.0f;
            v.normal[2] = 1.0f;
            v.weights[0] = 1.0f;
            verts->push_back(v);
        }
    }
    for (int y = 0; y < side - 1; ++y) {
        for (int x = 0; x < side - 1; ++x) {
            const uint32_t a = static_cast<uint32_t>(y * side + x);
            const uint32_t b = a + 1;
            const uint32_t c = a + static_cast<uint32_t>(side);
            const uint32_t d = c + 1;
            indices->insert(indices->end(), {a, b, c, b, d, c});
        }
    }
}
} /* namespace */

int main() {
    /* ---- the projection maths ---------------------------------------------------------- */
    /* world/pixel at distance d = 2*d*tan(fov/2)/height. At 3 m, fov 32, 2340 px:
       2*3*tan(16deg)/2340 = 7.352446e-4 m. */
    checkNear("worldPerPixel at the recorded phone framing",
              af::lodWorldPerPixel(3.0f, 32.0f, 2340.0f), 7.352446e-4f, 1e-7f);
    check("worldPerPixel is 0 for a non-positive distance", af::lodWorldPerPixel(0.0f, 32.0f, 2340.0f) == 0.0f);
    check("worldPerPixel is 0 for a zero field of view", af::lodWorldPerPixel(3.0f, 0.0f, 2340.0f) == 0.0f);
    check("worldPerPixel is 0 for a 180-degree field of view", af::lodWorldPerPixel(3.0f, 180.0f, 2340.0f) == 0.0f);
    check("worldPerPixel is 0 for a zero-height viewport", af::lodWorldPerPixel(3.0f, 32.0f, 0.0f) == 0.0f);

    /* Equilateral edge of area a is sqrt(4a/sqrt(3)). Area 1.0 -> edge 1.5197. */
    checkNear("triangleEdgeWorld of a unit-area equilateral", af::lodTriangleEdgeWorld(1.0f), 1.51967f, 1e-4f);
    check("triangleEdgeWorld is 0 for a non-positive area", af::lodTriangleEdgeWorld(0.0f) == 0.0f);

    /* ---- AvatarLodSet measures area ---------------------------------------------------- */
    {
        const std::vector<af_vertex> verts = unitSquareVerts();
        const std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};
        af_submesh sub{};
        sub.vertex_offset = 0;
        sub.vertex_count = static_cast<int32_t>(verts.size());
        sub.index_offset = 0;
        sub.index_count = static_cast<int32_t>(indices.size());
        af::AvatarLodSet lod;
        check("two-triangle square builds", lod.build(verts.data(), verts.size(), indices.data(), indices.size(), &sub, 1));
        check("measured areas are valid", lod.areasValid());
        checkNear("unit square measures 1.0 m^2", lod.surfaceAreaM2(0), 1.0f, 1e-5f);
        checkNear("mean triangle area of the square is 0.5 m^2", lod.meanTriangleAreaM2(0), 0.5f, 1e-5f);
        check("an out-of-range level reports 0 area", lod.surfaceAreaM2(9) == 0.0f);
    }

    /* A grid dense enough to produce coarser levels: every level's area is measured and the
       decimated levels keep roughly the same surface (index-only clustering over the same sheet). */
    {
        std::vector<af_vertex> verts;
        std::vector<uint32_t> indices;
        buildGrid(24, &verts, &indices);
        af_submesh sub{};
        sub.vertex_offset = 0;
        sub.vertex_count = static_cast<int32_t>(verts.size());
        sub.index_offset = 0;
        sub.index_count = static_cast<int32_t>(indices.size());
        af::AvatarLodSet lod;
        check("grid builds", lod.build(verts.data(), verts.size(), indices.data(), indices.size(), &sub, 1));
        check("grid areas are valid", lod.areasValid());
        checkNear("grid LOD0 area is the unit square", lod.surfaceAreaM2(0), 1.0f, 1e-4f);
        if (lod.levelCount() > 1) {
            const float a1 = lod.surfaceAreaM2(1);
            check("coarser level area is measured and positive", a1 > 0.0f);
            check("coarser level stays within a sane band of the surface", a1 > 0.5f && a1 < 1.6f);
            checkNear("coarser mean triangle area exceeds the finer one",
                      lod.meanTriangleAreaM2(1) > lod.meanTriangleAreaM2(0) ? 1.0f : 0.0f, 1.0f, 0.0f);
        }
    }

    /* ---- selection against the recorded phone view ------------------------------------- */
    {
        af::AvatarLodScreenMetrics m[3] = {phoneMetrics(0), phoneMetrics(1), phoneMetrics(2)};
        /* Full body at 3 m: LOD0 is ~2.6 px (below the 4.0 floor), LOD1 ~7.6 px. Finest that
           clears the floor is LOD1 -- the ~132k band repair R3 asks for, not the full 1.1 M. */
        const af::AvatarLodSelection sel = af::selectAvatarLodByScreenDensity(m, 3, -1);
        check("phone full-body selection is measured", sel.measured);
        check("phone full-body selects LOD1, not the full mesh", sel.level == 1);
        check("selected level clears the target", sel.pixelEdge >= af::kAvatarLodPixelEdgeTarget);
    }
    {
        /* Face zoom: same avatar at 1 m triples every projected size, so LOD0 (~7.9 px) clears the
           floor and full detail is earned back. */
        af::AvatarLodScreenMetrics m[3] = {phoneMetrics(0), phoneMetrics(1), phoneMetrics(2)};
        for (int i = 0; i < 3; ++i) m[i].viewDistanceM = 1.0f;
        const af::AvatarLodSelection sel = af::selectAvatarLodByScreenDensity(m, 3, -1);
        check("close-up selects the full-detail LOD0", sel.measured && sel.level == 0);
    }
    {
        /* Far view: at 12 m even LOD2 (~3.0 px) is below the floor, so the coarsest available wins
           -- nothing finer would help at that size. */
        af::AvatarLodScreenMetrics m[3] = {phoneMetrics(0), phoneMetrics(1), phoneMetrics(2)};
        for (int i = 0; i < 3; ++i) m[i].viewDistanceM = 12.0f;
        const af::AvatarLodSelection sel = af::selectAvatarLodByScreenDensity(m, 3, -1);
        check("distant view falls back to the coarsest level", sel.measured && sel.level == 2);
    }

    /* ---- hysteresis --------------------------------------------------------------------- */
    {
        /* At 1.8 m LOD0 is ~4.4 px: above the 4.0 floor (so a fresh frame picks LOD0) but below
           4.0*1.25 (so a frame already on LOD1 does not refine yet). Same numbers, two outcomes. */
        af::AvatarLodScreenMetrics m[3] = {phoneMetrics(0), phoneMetrics(1), phoneMetrics(2)};
        for (int i = 0; i < 3; ++i) m[i].viewDistanceM = 1.8f;
        const af::AvatarLodSelection fresh = af::selectAvatarLodByScreenDensity(m, 3, -1);
        const af::AvatarLodSelection held = af::selectAvatarLodByScreenDensity(m, 3, 1);
        check("borderline refine: a fresh frame takes LOD0", fresh.level == 0);
        check("borderline refine: a frame on LOD1 holds within the dead band", held.level == 1);
    }
    {
        /* At 2.2 m LOD0 is ~3.6 px: below the 4.0 floor (a fresh frame coarsens to LOD1) but still
           above 4.0/1.25 = 3.2 (so a frame on LOD0 does not coarsen yet). */
        af::AvatarLodScreenMetrics m[3] = {phoneMetrics(0), phoneMetrics(1), phoneMetrics(2)};
        for (int i = 0; i < 3; ++i) m[i].viewDistanceM = 2.2f;
        const af::AvatarLodSelection fresh = af::selectAvatarLodByScreenDensity(m, 3, -1);
        const af::AvatarLodSelection held = af::selectAvatarLodByScreenDensity(m, 3, 0);
        check("borderline coarsen: a fresh frame takes LOD1", fresh.level == 1);
        check("borderline coarsen: a frame on LOD0 holds within the dead band", held.level == 0);
    }

    /* ---- degenerate and fallback inputs -------------------------------------------------- */
    {
        af::AvatarLodScreenMetrics m[3] = {phoneMetrics(0), phoneMetrics(1), phoneMetrics(2)};
        m[1].meanTriAreaM2 = 0.0f; /* an unmeasurable level */
        const af::AvatarLodSelection sel = af::selectAvatarLodByScreenDensity(m, 3, -1);
        check("an unmeasurable level falls back to the source mesh", !sel.measured && sel.level == 0);
    }
    {
        af::AvatarLodScreenMetrics m[1] = {phoneMetrics(0)};
        const af::AvatarLodSelection sel = af::selectAvatarLodByScreenDensity(m, 1, -1);
        check("a single level is selected and measured", sel.measured && sel.level == 0);
    }
    check("null metrics fall back to level 0",
          !af::selectAvatarLodByScreenDensity(nullptr, 3, -1).measured);
    check("a non-positive level count falls back to level 0",
          !af::selectAvatarLodByScreenDensity(nullptr, 0, -1).measured);

    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
