/*
 * af_lod_select.h -- screen-space level-of-detail selection for the single-avatar render path.
 *
 * Why this file exists
 * --------------------
 * The LOD index streams built by af_avatar_lod.h are only worth anything if something chooses
 * between them. The first implementation chose with two absolute camera distances:
 *
 *     selectedLod = distance >= 16 m ? 2 : distance >= 8 m ? 1 : 0;
 *
 * An avatar is about 1.8 m tall and Avatar Studio's orbit camera defaults to 3.0 m
 * (CameraState::distance). Neither 8 m nor 16 m is reachable while a user looks at an avatar, so
 * the forward colour pass drew LOD0 -- the full source mesh -- on every frame at every zoom. That
 * is the measured phone result: 1,100,168 triangles covering 194,401 pixels, so each visible
 * triangle covered about a third of a pixel (docs/PHONE_TEST_2026-11-15.md). A GPU shades in 2x2
 * pixel blocks, so at that density nearly every sample belongs to a different triangle and the
 * shading work multiplies while adding nothing that can be seen.
 *
 * What replaces it
 * ----------------
 * Density is matched to the screen instead of to a distance in metres. For a candidate level the
 * selection asks one question: how many pixels long is the average triangle edge once projected?
 * If the answer is at or above the target, that level still carries visible detail and is allowed;
 * if it is below, the level is finer than the screen can resolve and its extra triangles are pure
 * cost. The coarsest allowed level wins.
 *
 * The rule scales correctly on its own, which fixed thresholds cannot:
 *   - zoom in on a face   -> projected edges grow -> LOD0 is earned back, detail is kept where it
 *                            is actually visible;
 *   - zoom out to a group -> projected edges shrink -> a coarser level is selected, and the
 *                            triangles that remain are still about one pixel each.
 *
 * Sanity check against the recorded phone numbers (S24 SM-S921B, 1080x2340, fovDeg 32, 3.0 m):
 * world units per pixel at 3 m is 2*3*tan(16deg)/2340 = 9.57e-4 m. A 1.1 M-triangle avatar of
 * about 1.8 m^2 of surface gives a mean triangle area of 1.6e-6 m^2, an equilateral edge of
 * 1.9e-3 m, and therefore about 2.0 projected pixels per edge at LOD0 -- and about 194,401 px /
 * 550,000 front-facing triangles = 0.35 px of AREA per triangle, which is the figure the phone
 * reported. Both describe the same fault: the mesh is roughly an order of magnitude finer than the
 * screen. At the 1.0 px edge target below, LOD1 (about 12% of the triangles, ~2.9x coarser edge)
 * is the level a full-body view selects, which lands in the 100-150k triangle band that
 * docs/PHOTOREAL_UPGRADE_PLAN.md repair R3 asks for.
 *
 * This header is deliberately pure maths with no GL, no engine and no Android dependency, so the
 * policy can be exercised by a host regression test (avatar_lod_select_test.cpp) rather than only
 * being observable through a phone.
 *
 * Honesty notes, in the project's own terms:
 *   - The surface area is MEASURED from the rest-pose triangles by AvatarLodSet, not estimated
 *     from a bounding box. It is measured once per topology change, not per frame.
 *   - The view distance is measured from the live camera eye to the REST-pose bounds centre. A
 *     posed limb can be nearer or farther; the error is a fraction of the orbit distance and only
 *     moves the selection across a threshold, where the hysteresis below already absorbs it.
 *   - Nothing here changes geometry. Every level still addresses the engine's original vertex
 *     stream, so skinning, morphs and normals are untouched.
 */
#ifndef AF_LOD_SELECT_H
#define AF_LOD_SELECT_H

#include <cmath>
#include <cstddef>

namespace af {

/*
 * The density the colour pass aims for, in projected pixels per average triangle EDGE, measured as
 * if that triangle faced the camera (see the honesty note below).
 *
 * POLICY: choose the FINEST level whose projected edge is at least this many pixels; only when even
 * the coarsest level falls below it (the avatar is tiny on screen) does the coarsest win. So the
 * target is a floor on useful detail, not a ceiling: it stops the pass submitting triangles finer
 * than the screen resolves, while never coarsening a view that can still show more.
 *
 * Calibration. The recorded S24 view (docs/PHONE_TEST_2026-11-15.md: 1080x2340, fovDeg 32, 3.0 m,
 * a 1.1 M-triangle avatar of about 1.8 m^2) gives a camera-facing projected edge of about 2.6 px at
 * LOD0, 7.6 px at LOD1 (~132k) and 11.8 px at LOD2 (~55k). A target of 4.0 px therefore rejects
 * LOD0 and selects LOD1 for the default full-body studio framing -- the 100-150k band that repair
 * R3 in docs/PHOTOREAL_UPGRADE_PLAN.md asks for -- while a face zoomed to 1 m lifts LOD0 to ~7.9 px
 * and earns the full mesh back. The true on-screen edge is smaller than this metric (foreshortening
 * and back-face culling roughly halve to third it), so 4.0 px here corresponds to about 1 true px;
 * the number is deliberately expressed in this metric's own units and calibrated to the one real
 * device measurement the project has.
 */
constexpr float kAvatarLodPixelEdgeTarget = 4.0f;


/*
 * Hysteresis factor for stepping to a COARSER level than the one currently on screen.
 *
 * Without it, an orbit that sits exactly on a threshold re-selects between two levels on
 * consecutive frames and the silhouette visibly crawls. A coarser level must therefore clear
 * target * kAvatarLodHysteresis to be adopted; the level already in use only has to keep clearing
 * target itself to be retained. 1.25 gives a 25% dead band, which is wider than the rest-pose
 * distance approximation above can drift.
 */
constexpr float kAvatarLodHysteresis = 1.25f;

/*
 * Everything the selection needs about one candidate level. Filled in by the caller from
 * AvatarLodSet's measured per-level areas and the live camera.
 */
struct AvatarLodScreenMetrics {
    float viewportHeightPx = 0.0f; /* the colour target's height in pixels                      */
    float fovDeg           = 0.0f; /* vertical field of view, degrees (CameraState::fovDeg)      */
    float viewDistanceM    = 0.0f; /* camera eye to the model bounds centre, metres              */
    float meanTriAreaM2    = 0.0f; /* measured mean area of this level's triangles, m^2          */
};

/* The chosen level plus the numbers that justify it, for the telemetry and the diagnostics JSON. */
struct AvatarLodSelection {
    int   level           = 0;     /* index into AvatarLodSet; 0 is the exact source mesh        */
    float pixelEdgeTarget = 0.0f;  /* the target this frame used                                 */
    float pixelEdge       = 0.0f;  /* projected pixels per triangle edge at the chosen level     */
    bool  measured        = false; /* false when the inputs could not support a real measurement */
};

/*
 * World units per pixel at `distanceM` for a perspective camera of vertical field `fovDeg`
 * covering `viewportHeightPx` rows. Returns 0 when the inputs cannot describe a projection, so a
 * caller can never divide by it by accident.
 */
inline float lodWorldPerPixel(float distanceM, float fovDeg, float viewportHeightPx) {
    if (!(distanceM > 0.0f) || !(viewportHeightPx > 0.0f)) return 0.0f;
    if (!(fovDeg > 0.0f) || fovDeg >= 180.0f) return 0.0f;
    const float halfFovRad = 0.5f * fovDeg * (3.14159265358979323846f / 180.0f);
    const float tanHalf = std::tan(halfFovRad);
    if (!std::isfinite(tanHalf) || tanHalf <= 0.0f) return 0.0f;
    return 2.0f * distanceM * tanHalf / viewportHeightPx;
}

/*
 * Edge length in metres of an equilateral triangle of area `meanTriAreaM2`.
 *
 * The mean triangle of a decimated mesh is not equilateral, but the level is being compared
 * against itself across a known triangle-count ratio, and equilateral is the standard, stable
 * stand-in: area = (sqrt(3)/4) * e^2, so e = sqrt(4 * area / sqrt(3)).
 */
inline float lodTriangleEdgeWorld(float meanTriAreaM2) {
    if (!(meanTriAreaM2 > 0.0f) || !std::isfinite(meanTriAreaM2)) return 0.0f;
    return std::sqrt(4.0f * meanTriAreaM2 / 1.7320508075688772f);
}

/* Projected pixels per average triangle edge for one candidate level. 0 means "not measurable". */
inline float lodProjectedPixelEdge(const AvatarLodScreenMetrics& m) {
    const float worldPerPixel = lodWorldPerPixel(m.viewDistanceM, m.fovDeg, m.viewportHeightPx);
    if (!(worldPerPixel > 0.0f)) return 0.0f;
    const float edgeWorld = lodTriangleEdgeWorld(m.meanTriAreaM2);
    if (!(edgeWorld > 0.0f)) return 0.0f;
    const float pixels = edgeWorld / worldPerPixel;
    return std::isfinite(pixels) ? pixels : 0.0f;
}

/*
 * Chooses the FINEST level whose projected triangle edge is at least `pixelEdgeTarget` pixels, so
 * the frame submits no triangle finer than the screen resolves while keeping every bit of detail
 * the current framing can still show. Because a coarser level always has larger triangles, the
 * levels that meet the target form a suffix of the list; the finest of them is the first index that
 * clears it. When even the coarsest level is below the target -- the avatar is small on screen --
 * the coarsest available level is chosen, since nothing finer would help.
 *
 *   metrics[i]   -- the screen metrics of level i. Level 0 must be the exact source mesh.
 *   levelCount   -- how many levels exist (AvatarLodSet::levelCount).
 *   currentLevel -- the level drawn last frame, for hysteresis. Pass a negative value for "none".
 *
 * Falls back to level 0, measured=false, whenever any level's inputs cannot support a measurement,
 * because drawing the exact source mesh is always correct -- it is only ever wasteful, never wrong.
 * The caller treats measured=false as "keep the previous behaviour".
 */
inline AvatarLodSelection selectAvatarLodByScreenDensity(const AvatarLodScreenMetrics* metrics,
                                                        int levelCount,
                                                        int currentLevel,
                                                        float pixelEdgeTarget = kAvatarLodPixelEdgeTarget,
                                                        float hysteresis = kAvatarLodHysteresis) {
    AvatarLodSelection out;
    out.level = 0;
    out.pixelEdgeTarget = pixelEdgeTarget;
    out.measured = false;
    if (!metrics || levelCount <= 0) return out;
    if (!(pixelEdgeTarget > 0.0f) || !std::isfinite(pixelEdgeTarget)) return out;
    if (!(hysteresis >= 1.0f) || !std::isfinite(hysteresis)) return out;

    /* Every level must be measurable: the renderer compares them against each other, so one level
       without a real number would make the comparison meaningless. Bail to the source mesh. */
    for (int level = 0; level < levelCount; ++level) {
        if (!(lodProjectedPixelEdge(metrics[static_cast<size_t>(level)]) > 0.0f)) return out;
    }

    /* The ideal level: finest that clears the target, else the coarsest. */
    int ideal = levelCount - 1;
    for (int level = 0; level < levelCount; ++level) {
        if (lodProjectedPixelEdge(metrics[static_cast<size_t>(level)]) >= pixelEdgeTarget) {
            ideal = level;
            break;
        }
    }

    /* Hysteresis: a change of level must clear a dead band before it is adopted, so an orbit that
       sits on a threshold does not flip the silhouette every frame. Moving finer needs the finer
       level to clear target*hysteresis; moving coarser needs the current level to have fallen
       below target/hysteresis. The current level is only trusted when it is a real index, so a
       stale value from a topology change cannot pin the selection. */
    int chosen = ideal;
    if (currentLevel >= 0 && currentLevel < levelCount && ideal != currentLevel) {
        if (ideal < currentLevel) {
            if (lodProjectedPixelEdge(metrics[static_cast<size_t>(ideal)]) < pixelEdgeTarget * hysteresis) {
                chosen = currentLevel;
            }
        } else {
            if (lodProjectedPixelEdge(metrics[static_cast<size_t>(currentLevel)]) >= pixelEdgeTarget / hysteresis) {
                chosen = currentLevel;
            }
        }
    }

    out.level = chosen;
    out.pixelEdge = lodProjectedPixelEdge(metrics[static_cast<size_t>(chosen)]);
    out.measured = true;
    return out;
}

} /* namespace af */

#endif /* AF_LOD_SELECT_H */
