/*
 * af_hair_cards.cpp -- procedural layered strand hair card generator.
 *
 * Architecture:
 *   * Every hair card is a watertight, outward-wound closed thin tapered V-arched 3D ribbon
 *     (4 vertices per ring around the ribbon perimeter, capped cleanly at the root and pinched
 *     to a closed apex at the tip). This guarantees 0 open edges and 0 non-manifold edges so
 *     the body mesh remains 100% manifold while giving real silhouette depth from every angle.
 *   * Guide curves are pushed clear of the head & body SDFs (`field.head()` / `field.body()`)
 *     so strands drape naturally over the cranium, cheeks, neck, and shoulders without clipping.
 *   * Parametric UVs `(u_variant, v_root_to_tip)` are written directly into `mesh->uvs` for
 *     REGION_HAIR_CARD vertices. `af_uv.cpp` packs all hair cards into one dedicated high-texel
 *     4-variant strand sheet tile, preserving root-to-tip flow and strand-aligned tangents.
 */
#include "af_hair_cards.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace af {
namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

/** Deterministic [0, 1) hash so strand jitter is reproducible across runs and platforms. */
float hash01(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return static_cast<float>(x & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float hashSigned(uint32_t x) {
    return hash01(x) * 2.0f - 1.0f;
}

/** Finite-difference outward normal of head + body collision field at world point `p`. */
Vec3 collisionNormal(const BodyField& field, const Vec3& p) {
    const float h = 0.0035f;
    auto sdf = [&](const Vec3& q) {
        return minf(field.head(q), field.body(q));
    };
    const Vec3 g(
        sdf(Vec3(p.x + h, p.y, p.z)) - sdf(Vec3(p.x - h, p.y, p.z)),
        sdf(Vec3(p.x, p.y + h, p.z)) - sdf(Vec3(p.x, p.y - h, p.z)),
        sdf(Vec3(p.x, p.y, p.z + h)) - sdf(Vec3(p.x, p.y, p.z - h))
    );
    return g.normalizedOr(Vec3(0.0f, 1.0f, 0.0f));
}

/** Pushes a strand point clear of skin/skull/shoulders by at least `clearance` metres. */
Vec3 pushClearOfBody(const BodyField& field, Vec3 p, float clearance) {
    for (int iter = 0; iter < 3; ++iter) {
        const float d = minf(field.head(p), field.body(p));
        if (d >= clearance) break;
        const Vec3 n = collisionNormal(field, p);
        p += n * (clearance - d + 0.0005f);
    }
    return p;
}

/** Projects a scalp anchor onto the outer hair/skull surface so roots nest cleanly into the cap. */
Vec3 anchorOnScalp(const BodyField& field, const Vec3& headCenter, const Vec3& dir, float lift) {
    const BodyMetrics& m = field.metrics();
    const float headH = m.y.crownY - m.y.chinY;
    const Vec3 radii(m.head.rx * 1.02f, headH * 0.50f, m.head.rz * 1.02f);
    Vec3 p(headCenter.x + dir.x * radii.x,
           headCenter.y + dir.y * radii.y,
           headCenter.z + dir.z * radii.z);
    /* Refine onto the actual head SDF + lift. */
    for (int iter = 0; iter < 3; ++iter) {
        const float d = field.head(p);
        const Vec3 n = collisionNormal(field, p);
        p += n * (lift - d);
    }
    return p;
}

struct RibbonParams {
    float rootWidth = 0.022f;
    float midWidth = 0.026f;
    float tipWidth = 0.004f;
    float thickness = 0.0018f;
    float archHeight = 0.0032f;
    int variant = 0;
};

/**
 * Appends one watertight, outward-wound closed tapered V-arched strand ribbon along `spine`.
 * Cross-section per ring (4 vertices, CCW when viewed looking from root toward tip):
 *   0: left edge        (-side * halfW)
 *   1: outer crest      (+out  * (halfT + arch))
 *   2: right edge       (+side * halfW)
 *   3: inner underside  (-out  * halfT)
 * Root is closed by a 4-triangle fan around the root center; tip pinches cleanly to a single
 * apex vertex so every edge is shared by exactly two oppositely-oriented half-edges.
 */
void appendWatertightRibbon(Mesh* mesh, const std::vector<Vec3>& spine,
                            const std::vector<Vec3>& outDirs,
                            const RibbonParams& rp, float floorShiftY,
                            HairCardReport* report) {
    const int n = static_cast<int>(spine.size());
    if (n < 3) return;

    const float uSpan = 1.0f / static_cast<float>(kHairCardVariants);
    const float uPad = 0.012f * uSpan;
    const float u0 = static_cast<float>(rp.variant % kHairCardVariants) * uSpan + uPad;
    const float u1 = static_cast<float>((rp.variant % kHairCardVariants) + 1) * uSpan - uPad;
    const float uMid = 0.5f * (u0 + u1);

    const uint32_t baseVertex = static_cast<uint32_t>(mesh->positions.size());
    auto pushV = [&](const Vec3& pos, const Vec3& norm, float u, float v) {
        mesh->positions.push_back(pos + Vec3(0.0f, floorShiftY, 0.0f));
        mesh->normals.push_back(norm.normalizedOr(Vec3(0.0f, 1.0f, 0.0f)));
        mesh->regions.push_back(REGION_HAIR_CARD);
        mesh->uvs.push_back(u);
        mesh->uvs.push_back(v);
    };

    /* Compute parallel-transported ribbon frames along the spine. */
    std::vector<Vec3> tangents(static_cast<size_t>(n));
    std::vector<Vec3> outs(static_cast<size_t>(n));
    std::vector<Vec3> sides(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        Vec3 t;
        if (i == 0) t = spine[1] - spine[0];
        else if (i == n - 1) t = spine[static_cast<size_t>(n - 1)] - spine[static_cast<size_t>(n - 2)];
        else t = spine[static_cast<size_t>(i + 1)] - spine[static_cast<size_t>(i - 1)];
        t = t.normalizedOr(Vec3(0.0f, -1.0f, 0.0f));

        Vec3 o = outDirs[static_cast<size_t>(i)];
        o = (o - t * o.dot(t)).normalizedOr(Vec3(0.0f, 0.0f, 1.0f));
        if (i > 0 && o.dot(outs[static_cast<size_t>(i - 1)]) < 0.0f) o = o * -1.0f;

        /* Right-handed basis: side = cross(out, tangent) -> +side is right when facing outer side
           looking root->tip, guaranteeing CCW outward normals on all 4 ribbon walls. */
        Vec3 s = o.cross(t).normalizedOr(Vec3(1.0f, 0.0f, 0.0f));
        o = t.cross(s).normalizedOr(o);

        tangents[static_cast<size_t>(i)] = t;
        outs[static_cast<size_t>(i)] = o;
        sides[static_cast<size_t>(i)] = s;
    }

    /* Vertex 0: root cap center. */
    pushV(spine[0], tangents[0] * -1.0f, uMid, 0.0f);

    /* Rings 0 .. n-2 (4 vertices each). Ring n-1 collapses to a single closed tip apex. */
    const int ringCount = n - 1;
    for (int i = 0; i < ringCount; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(n - 1);
        /* Smooth width profile: slightly wider through mid-body, gracefully tapered to tip. */
        const float wRootMid = lerpf(rp.rootWidth, rp.midWidth, smoothstepf(0.0f, 0.30f, v));
        const float w = lerpf(wRootMid, rp.tipWidth, smoothstepf(0.55f, 1.0f, v));
        const float halfW = maxf(w * 0.5f, 0.0012f);
        const float taperT = lerpf(1.0f, 0.35f, v);
        const float halfT = maxf(rp.thickness * 0.5f * taperT, 0.00035f);
        const float arch = rp.archHeight * taperT;

        const Vec3& c = spine[static_cast<size_t>(i)];
        const Vec3& s = sides[static_cast<size_t>(i)];
        const Vec3& o = outs[static_cast<size_t>(i)];

        /* 0: left edge, 1: outer crest, 2: right edge, 3: inner underside */
        pushV(c - s * halfW,             (o * 0.45f - s * 0.89f), u0,   v);
        pushV(c + o * (halfT + arch),    o,                       uMid, v);
        pushV(c + s * halfW,             (o * 0.45f + s * 0.89f), u1,   v);
        pushV(c - o * halfT,             o * -1.0f,               uMid, v);
    }

    /* Final vertex: closed tip apex. */
    const uint32_t tipIdx = static_cast<uint32_t>(mesh->positions.size());
    pushV(spine[static_cast<size_t>(n - 1)], tangents[static_cast<size_t>(n - 1)], uMid, 1.0f);

    auto ringV = [&](int r, int k) -> uint32_t {
        return baseVertex + 1u + static_cast<uint32_t>(r * 4 + (k & 3));
    };

    const size_t triBefore = mesh->indices.size() / 3;

    /* Root cap (outward normal points along -tangent). */
    for (int k = 0; k < 4; ++k) {
        mesh->indices.push_back(baseVertex);
        mesh->indices.push_back(ringV(0, k + 1));
        mesh->indices.push_back(ringV(0, k));
    }

    /* Ribbon shaft quads between consecutive rings (outward CCW). */
    for (int r = 0; r < ringCount - 1; ++r) {
        for (int k = 0; k < 4; ++k) {
            const uint32_t a = ringV(r, k);
            const uint32_t b = ringV(r, k + 1);
            const uint32_t c = ringV(r + 1, k);
            const uint32_t d = ringV(r + 1, k + 1);
            mesh->indices.push_back(a);
            mesh->indices.push_back(b);
            mesh->indices.push_back(c);
            mesh->indices.push_back(b);
            mesh->indices.push_back(d);
            mesh->indices.push_back(c);
        }
    }

    /* Tip cap (converges to apex, outward CCW). */
    const int lastR = ringCount - 1;
    for (int k = 0; k < 4; ++k) {
        mesh->indices.push_back(ringV(lastR, k));
        mesh->indices.push_back(ringV(lastR, k + 1));
        mesh->indices.push_back(tipIdx);
    }

    if (report) {
        report->cardCount += 1;
        report->vertexCount += static_cast<int>(mesh->positions.size() - baseVertex);
        report->triangleCount += static_cast<int>(mesh->indices.size() / 3 - triBefore);
    }
}

/** Builds a draped hanging/sweeping strand from a scalp root with gravity, wave, and SDF collision. */
void buildDrapedCard(const BodyField& field, const Vec3& headCenter,
                     const Vec3& rootDir, float length, float lift,
                     float backBias, float sideSweep, float waveAmp, float waveFreq,
                     float phase, int segments, const RibbonParams& rp,
                     float floorShiftY, Mesh* mesh, HairCardReport* report) {
    const BodyMetrics& m = field.metrics();
    const Vec3 root = anchorOnScalp(field, headCenter, rootDir, lift);

    /* Initial flow tangent along scalp away from the top crown part. */
    Vec3 scalpOut = collisionNormal(field, root);
    Vec3 down(0.0f, -1.0f, 0.0f);
    Vec3 initialDir = (down + Vec3(sideSweep, -0.25f, backBias));
    /* Keep root tangent roughly tangential to the skull with slight volume lift. */
    initialDir = (initialDir - scalpOut * (initialDir.dot(scalpOut) * 0.72f) + scalpOut * 0.16f)
        .normalizedOr(down);

    std::vector<Vec3> spine;
    std::vector<Vec3> outs;
    spine.reserve(static_cast<size_t>(segments + 1));
    outs.reserve(static_cast<size_t>(segments + 1));

    Vec3 cur = root;
    Vec3 dir = initialDir;
    const float step = length / static_cast<float>(segments);

    spine.push_back(cur);
    outs.push_back(scalpOut);

    for (int s = 1; s <= segments; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(segments);
        /* Blend smoothly from scalp-hugging trajectory into gravity drape + subtle S-wave. */
        const float wave = std::sin(t * waveFreq * kTwoPi + phase) * waveAmp * smoothstepf(0.15f, 0.85f, t);
        const Vec3 sideAxis = Vec3(dir.z, 0.0f, -dir.x).normalizedOr(Vec3(1.0f, 0.0f, 0.0f));
        Vec3 targetDir = (Vec3(sideSweep * (1.0f - 0.65f * t), -1.0f, backBias * (1.0f - 0.55f * t))
                          + sideAxis * wave).normalizedOr(down);
        dir = dir.lerp(targetDir, 0.42f).normalizedOr(down);

        Vec3 next = cur + dir * step;
        /* Keep every hanging lock clear of the skull, face, neck, and shoulders. */
        const float minClear = lift + 0.0025f + 0.004f * t;
        next = pushClearOfBody(field, next, minClear);

        /* Ensure front-facing strands stay clear of the eyes & nose bridge. */
        const float eyeY = field.eyeCentre(1).y;
        const float browZ = field.browRidge_.z;
        if (next.z < browZ + 0.015f && next.y < field.browRidge_.y + 0.015f && next.y > m.y.chinY - 0.03f) {
            const float minAbsX = m.head.rx * 0.72f;
            if (std::fabs(next.x) < minAbsX) {
                const float signX = (rootDir.x >= 0.0f) ? 1.0f : -1.0f;
                next.x = signX * minAbsX;
                next = pushClearOfBody(field, next, minClear);
            }
        }
        (void)eyeY;

        Vec3 outN = collisionNormal(field, next);
        /* For lower hanging tips far from the torso, bias ribbon outer normal radially away from spine. */
        Vec3 radial(next.x - headCenter.x, 0.08f, next.z - headCenter.z);
        if (radial.lengthSq() > 1e-5f) {
            outN = outN.lerp(radial.normalized(), smoothstepf(0.25f, 0.80f, t)).normalizedOr(outN);
        }

        dir = (next - cur).normalizedOr(dir);
        cur = next;
        spine.push_back(cur);
        outs.push_back(outN);
    }

    appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, report);
}

/** Generates swept-back scalp crown cards that converge cleanly to a rear tie/bun anchor. */
void buildSweptCrownToAnchor(const BodyField& field, const Vec3& headCenter,
                             const Vec3& anchor, int count, float width,
                             float floorShiftY, Mesh* mesh, HairCardReport* report) {
    const BodyMetrics& m = field.metrics();
    const float headH = m.y.crownY - m.y.chinY;
    for (int i = 0; i < count; ++i) {
        const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        /* Distribute roots around the hairline arc (temple -> crown -> opposite temple -> nape). */
        const float az = -kPi * 0.72f + u * (kPi * 1.44f);
        const float elev = 0.25f + 0.50f * std::cos(az * 0.65f);
        const Vec3 dir(std::sin(az) * std::cos(elev),
                       std::sin(elev),
                       -std::cos(az) * std::cos(elev));
        const Vec3 root = anchorOnScalp(field, headCenter, dir, 0.006f + 0.002f * hash01(static_cast<uint32_t>(i * 37 + 5)));
        constexpr int kSegs = 7;
        std::vector<Vec3> spine;
        std::vector<Vec3> outs;
        spine.reserve(kSegs + 1);
        outs.reserve(kSegs + 1);
        for (int s = 0; s <= kSegs; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(kSegs);
            /* Lift mid-arc slightly above the skull so strands sweep smoothly over the cranium. */
            Vec3 p = root.lerp(anchor, t);
            p.y += std::sin(t * kPi) * headH * 0.06f;
            p = pushClearOfBody(field, p, 0.006f + 0.003f * (1.0f - t));
            spine.push_back(p);
            outs.push_back(collisionNormal(field, p));
        }
        RibbonParams rp{};
        rp.rootWidth = width * 0.90f;
        rp.midWidth = width;
        rp.tipWidth = width * 0.35f;
        rp.thickness = 0.0016f;
        rp.archHeight = 0.0024f;
        rp.variant = i % kHairCardVariants;
        appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, report);
    }
}

} // namespace

HairCardReport appendProceduralHairCards(const BodyField& field, float floorShiftY, Mesh* mesh) {
    HairCardReport report{};
    if (!mesh) return report;
    const int style = field.apparel().hair;
    if (style <= 0) return report;

    /* Ensure `mesh->uvs` exists and aligns with existing vertices before appending card UVs. */
    if (mesh->uvs.size() < mesh->positions.size() * 2) {
        mesh->uvs.resize(mesh->positions.size() * 2, 0.0f);
    }

    const BodyMetrics& m = field.metrics();
    const float headH = maxf(m.y.crownY - m.y.chinY, 0.16f);
    const Vec3 headCenter(0.0f, (m.y.crownY + m.y.chinY) * 0.54f, m.head.centreZ);
    uint32_t cardSeed = 1009u + static_cast<uint32_t>(style) * 7919u;

    auto nextVariant = [&]() -> int {
        cardSeed = cardSeed * 1664525u + 1013904223u;
        return static_cast<int>((cardSeed >> 24) % static_cast<uint32_t>(kHairCardVariants));
    };

    switch (style) {
        case 1: { /* BUZZ: directional short cropped crown/temple cards */
            const int rings = 3;
            for (int r = 0; r < rings; ++r) {
                const float elev = lerpf(0.75f, 0.28f, static_cast<float>(r) / static_cast<float>(rings - 1));
                const int count = 10 + r * 4;
                for (int i = 0; i < count; ++i) {
                    const uint32_t h = static_cast<uint32_t>(r * 131 + i * 17 + 3);
                    const float az = -kPi * 0.85f + (static_cast<float>(i) + 0.5f) / static_cast<float>(count) * (kPi * 1.70f);
                    const Vec3 dir(std::sin(az) * std::cos(elev),
                                   std::sin(elev),
                                   std::cos(az) * std::cos(elev));
                    RibbonParams rp{};
                    rp.rootWidth = m.head.rx * 0.24f;
                    rp.midWidth = m.head.rx * 0.26f;
                    rp.tipWidth = m.head.rx * 0.08f;
                    rp.thickness = 0.0012f;
                    rp.archHeight = 0.0018f;
                    rp.variant = nextVariant();
                    buildDrapedCard(field, headCenter, dir, headH * (0.14f + 0.03f * hash01(h)),
                                    0.0045f + 0.0015f * static_cast<float>(r),
                                    0.25f, std::sin(az) * 0.15f, 0.02f, 0.5f, hash01(h + 9) * kTwoPi,
                                    4, rp, floorShiftY, mesh, &report);
                }
            }
            break;
        }

        case 2: { /* SHORT: layered textured crown + side-swept fringe + tapered nape */
            const int tiers = 3;
            for (int tier = 0; tier < tiers; ++tier) {
                const float elev = lerpf(0.78f, 0.18f, static_cast<float>(tier) / static_cast<float>(tiers - 1));
                const int count = 12 + tier * 5;
                for (int i = 0; i < count; ++i) {
                    const uint32_t h = static_cast<uint32_t>(tier * 211 + i * 29 + 11);
                    const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
                    /* Full wrap around back and temples, leaving face open below brow. */
                    const float az = -kPi * 0.78f + u * (kPi * 1.56f);
                    const Vec3 dir(std::sin(az) * std::cos(elev),
                                   std::sin(elev),
                                   std::cos(az) * std::cos(elev));
                    const bool isFrontSide = std::fabs(az) > kPi * 0.52f;
                    const float len = headH * (isFrontSide ? 0.30f : 0.36f) * (0.85f + 0.30f * hash01(h));
                    RibbonParams rp{};
                    rp.rootWidth = m.head.rx * 0.28f;
                    rp.midWidth = m.head.rx * 0.32f;
                    rp.tipWidth = m.head.rx * 0.06f;
                    rp.thickness = 0.0016f;
                    rp.archHeight = 0.0028f;
                    rp.variant = nextVariant();
                    buildDrapedCard(field, headCenter, dir, len,
                                    0.007f + 0.003f * static_cast<float>(tier),
                                    0.32f, std::sin(az) * 0.28f + 0.08f * hashSigned(h + 3),
                                    0.08f, 1.0f, hash01(h + 7) * kTwoPi,
                                    6, rp, floorShiftY, mesh, &report);
                }
            }
            break;
        }

        case 3:   /* MEDIUM: jaw/shoulder layered locks with face-framing strands */
        case 4: { /* LONG: multi-layer cascading curtain + cheek/shoulder framing locks */
            const bool isLong = (style == 4);
            const float baseLen = isLong ? headH * 2.35f : headH * 0.78f;
            const int segs = isLong ? 11 : 8;

            /* Layer 1 (inner volume curtain) + Layer 2 (mid body waves) + Layer 3 (outer wispy crown). */
            for (int layer = 0; layer < 3; ++layer) {
                const float elev = (layer == 0) ? 0.22f : ((layer == 1) ? 0.52f : 0.80f);
                const int count = (layer == 0) ? 18 : ((layer == 1) ? 20 : 16);
                const float lift = 0.008f + 0.0055f * static_cast<float>(layer);
                for (int i = 0; i < count; ++i) {
                    const uint32_t h = static_cast<uint32_t>(layer * 317 + i * 43 + 19);
                    const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
                    /* Azimuth 0 = straight back (+Z), +/-0.74*pi = front temples near forehead. */
                    const float az = -kPi * 0.74f + u * (kPi * 1.48f);
                    const float azJitter = az + 0.045f * hashSigned(h);
                    const Vec3 dir(std::sin(azJitter) * std::cos(elev),
                                   std::sin(elev),
                                   std::cos(azJitter) * std::cos(elev));

                    const float frontFactor = smoothstepf(0.45f * kPi, 0.76f * kPi, std::fabs(azJitter));
                    /* Face-framing side locks are slightly shorter and more tapered than rear locks. */
                    const float lenScale = lerpf(1.0f, isLong ? 0.62f : 0.78f, frontFactor)
                                         * (0.86f + 0.26f * hash01(h + 1));
                    const float len = baseLen * lenScale * (layer == 2 ? 1.04f : (layer == 1 ? 0.98f : 0.88f));
                    const float widthScale = (layer == 2) ? 0.82f : ((layer == 1) ? 1.00f : 1.15f);

                    RibbonParams rp{};
                    rp.rootWidth = m.head.rx * 0.28f * widthScale;
                    rp.midWidth = m.head.rx * 0.35f * widthScale;
                    rp.tipWidth = m.head.rx * 0.055f * widthScale;
                    rp.thickness = 0.0017f;
                    rp.archHeight = 0.0034f * widthScale;
                    rp.variant = nextVariant();

                    const float sideSweep = std::sin(azJitter) * (0.24f + 0.14f * frontFactor);
                    const float backBias = lerpf(0.38f, 0.12f, frontFactor);
                    const float waveAmp = isLong ? (0.14f + 0.06f * hash01(h + 5)) : 0.09f;
                    const float waveFreq = isLong ? (1.45f + 0.35f * hash01(h + 7)) : 0.95f;

                    buildDrapedCard(field, headCenter, dir, len, lift, backBias, sideSweep,
                                    waveAmp, waveFreq, hash01(h + 13) * kTwoPi,
                                    segs, rp, floorShiftY, mesh, &report);
                }
            }
            break;
        }

        case 5: { /* PONYTAIL: swept scalp cards + cascading arched ponytail ribbons */
            const Vec3 tie(0.0f, m.y.crownY - headH * 0.14f, m.head.centreZ + m.head.rz * 1.08f);
            buildSweptCrownToAnchor(field, headCenter, tie, 18, m.head.rx * 0.28f, floorShiftY, mesh, &report);

            /* Cascading ponytail ribbons radiating out of the tie point. */
            constexpr int kTailCards = 20;
            for (int i = 0; i < kTailCards; ++i) {
                const uint32_t h = static_cast<uint32_t>(i * 73 + 501);
                const float angle = (static_cast<float>(i) / static_cast<float>(kTailCards)) * kTwoPi;
                const float radial = (i < 8) ? 0.008f : 0.017f;
                const float len = headH * (1.35f + 0.45f * hash01(h));
                constexpr int kSegs = 9;
                std::vector<Vec3> spine;
                std::vector<Vec3> outs;
                spine.reserve(kSegs + 1);
                outs.reserve(kSegs + 1);
                const Vec3 radialDir(std::cos(angle), std::sin(angle) * 0.45f, std::fabs(std::sin(angle)) * 0.5f + 0.35f);
                const Vec3 rNorm = radialDir.normalizedOr(Vec3(0.0f, 0.0f, 1.0f));
                for (int s = 0; s <= kSegs; ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(kSegs);
                    /* Graceful parabolic arch away from the skull then down behind the back. */
                    const float spread = radial + (0.022f + 0.018f * hash01(h + 3)) * std::sin(t * kPi * 0.85f);
                    const float wave = std::sin(t * kTwoPi * 1.2f + angle) * 0.008f * t;
                    Vec3 p = tie
                           + Vec3(rNorm.x * spread + wave,
                                  std::sin(t * kPi * 0.65f) * 0.022f - t * len,
                                  0.042f * std::pow(t, 0.55f) + rNorm.z * spread * 0.65f);
                    p = pushClearOfBody(field, p, 0.008f);
                    spine.push_back(p);
                    outs.push_back((rNorm + Vec3(0.0f, 0.25f * (1.0f - t), 0.35f)).normalizedOr(Vec3(0.0f, 0.0f, 1.0f)));
                }
                RibbonParams rp{};
                rp.rootWidth = m.head.rx * 0.20f;
                rp.midWidth = m.head.rx * 0.30f;
                rp.tipWidth = m.head.rx * 0.05f;
                rp.thickness = 0.0017f;
                rp.archHeight = 0.0032f;
                rp.variant = nextVariant();
                appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, &report);
            }
            break;
        }

        case 6: { /* BUN: swept scalp cards + spiraled interwoven bun wrap cards */
            const Vec3 bunCenter(0.0f, m.y.crownY - headH * 0.16f, m.head.centreZ + m.head.rz * 1.04f);
            buildSweptCrownToAnchor(field, headCenter, bunCenter, 18, m.head.rx * 0.28f, floorShiftY, mesh, &report);

            /* Spiraled wrap ribbons encircling the bun ellipsoid. */
            constexpr int kWrapCards = 14;
            const Vec3 bunRadii(0.056f, 0.046f, 0.042f);
            for (int i = 0; i < kWrapCards; ++i) {
                const float startAngle = (static_cast<float>(i) / static_cast<float>(kWrapCards)) * kTwoPi;
                const float tilt = -0.35f + 0.70f * hash01(static_cast<uint32_t>(i * 53 + 9));
                constexpr int kSegs = 8;
                std::vector<Vec3> spine;
                std::vector<Vec3> outs;
                spine.reserve(kSegs + 1);
                outs.reserve(kSegs + 1);
                for (int s = 0; s <= kSegs; ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(kSegs);
                    const float a = startAngle + t * kPi * 1.35f;
                    const float ringLift = 1.04f + 0.08f * std::sin(t * kPi);
                    const Vec3 local(std::cos(a) * bunRadii.x * ringLift,
                                     (std::sin(a) * std::cos(tilt) + 0.25f * (t - 0.5f)) * bunRadii.y * ringLift,
                                     (0.35f + 0.65f * std::sin(a + 0.8f)) * bunRadii.z * ringLift);
                    const Vec3 p = bunCenter + local;
                    spine.push_back(p);
                    outs.push_back(local.normalizedOr(Vec3(0.0f, 0.0f, 1.0f)));
                }
                RibbonParams rp{};
                rp.rootWidth = 0.018f;
                rp.midWidth = 0.024f;
                rp.tipWidth = 0.008f;
                rp.thickness = 0.0016f;
                rp.archHeight = 0.0028f;
                rp.variant = nextVariant();
                appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, &report);
            }
            break;
        }

        case 7: { /* BRAIDS: parted scalp cards + twin helical three-strand braided ribbons */
            for (int side : {-1, 1}) {
                const Vec3 braidTop(side * m.head.rx * 0.80f,
                                    m.y.crownY - headH * 0.34f,
                                    m.head.centreZ + m.head.rz * 0.44f);
                /* Scalp sweep into each side braid root. */
                for (int i = 0; i < 8; ++i) {
                    const float u = (static_cast<float>(i) + 0.5f) / 8.0f;
                    const float az = side * (0.08f + u * (kPi * 0.68f));
                    const float elev = 0.32f + 0.45f * (1.0f - u * 0.6f);
                    const Vec3 dir(std::sin(az) * std::cos(elev),
                                   std::sin(elev),
                                   std::cos(az) * std::cos(elev));
                    const Vec3 root = anchorOnScalp(field, headCenter, dir, 0.007f);
                    constexpr int kSegs = 6;
                    std::vector<Vec3> spine, outs;
                    spine.reserve(kSegs + 1);
                    outs.reserve(kSegs + 1);
                    for (int s = 0; s <= kSegs; ++s) {
                        const float t = static_cast<float>(s) / static_cast<float>(kSegs);
                        Vec3 p = pushClearOfBody(field, root.lerp(braidTop, t), 0.007f);
                        spine.push_back(p);
                        outs.push_back(collisionNormal(field, p));
                    }
                    RibbonParams rp{};
                    rp.rootWidth = m.head.rx * 0.24f;
                    rp.midWidth = m.head.rx * 0.26f;
                    rp.tipWidth = m.head.rx * 0.12f;
                    rp.thickness = 0.0016f;
                    rp.archHeight = 0.0024f;
                    rp.variant = nextVariant();
                    appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, &report);
                }

                /* Three interwoven helical plait strands per side + tassel wisps. */
                constexpr int kStrands = 3;
                constexpr int kSegs = 16;
                const float braidLen = 0.36f;
                for (int st = 0; st < kStrands; ++st) {
                    const float phase = static_cast<float>(st) * (kTwoPi / 3.0f);
                    std::vector<Vec3> spine, outs;
                    spine.reserve(kSegs + 1);
                    outs.reserve(kSegs + 1);
                    for (int s = 0; s <= kSegs; ++s) {
                        const float t = static_cast<float>(s) / static_cast<float>(kSegs);
                        const float helixR = lerpf(0.015f, 0.005f, t);
                        const float theta = t * kTwoPi * 4.5f + phase;
                        Vec3 p(braidTop.x + side * 0.014f * t + std::cos(theta) * helixR,
                               braidTop.y - braidLen * t,
                               braidTop.z + 0.032f * t + std::sin(theta) * helixR);
                        p = pushClearOfBody(field, p, 0.008f);
                        spine.push_back(p);
                        outs.push_back(Vec3(std::cos(theta) + side * 0.35f, 0.1f, std::sin(theta) + 0.35f)
                                           .normalizedOr(Vec3(0.0f, 0.0f, 1.0f)));
                    }
                    RibbonParams rp{};
                    rp.rootWidth = 0.016f;
                    rp.midWidth = 0.014f;
                    rp.tipWidth = 0.004f;
                    rp.thickness = 0.0022f;
                    rp.archHeight = 0.0038f;
                    rp.variant = nextVariant();
                    appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, &report);
                }
            }
            break;
        }

        case 8: { /* AFRO: spherical multi-tier coiled ribbon tufts around crown/temples/nape */
            constexpr int kTufts = 48;
            const float goldenAngle = 2.39996323f;
            for (int i = 0; i < kTufts; ++i) {
                const uint32_t h = static_cast<uint32_t>(i * 97 + 331);
                /* Fibonacci hemisphere biased over the scalp above brow & jaw. */
                const float yFrac = lerpf(0.95f, -0.15f, (static_cast<float>(i) + 0.5f) / static_cast<float>(kTufts));
                const float rHoriz = std::sqrt(maxf(0.0f, 1.0f - yFrac * yFrac));
                const float phi = static_cast<float>(i) * goldenAngle;
                const Vec3 dir(rHoriz * std::sin(phi), yFrac, rHoriz * std::cos(phi));
                /* Keep face area clear (-Z forward, below hairline). */
                if (dir.z < -0.35f && dir.y < 0.42f) continue;

                const Vec3 root = anchorOnScalp(field, headCenter, dir, 0.018f);
                const Vec3 radialOut = dir.normalizedOr(Vec3(0.0f, 1.0f, 0.0f));
                Vec3 tangentA = radialOut.cross(Vec3(0.0f, 1.0f, 0.0f)).normalizedOr(Vec3(1.0f, 0.0f, 0.0f));
                Vec3 tangentB = radialOut.cross(tangentA).normalizedOr(Vec3(0.0f, 0.0f, 1.0f));

                constexpr int kSegs = 6;
                const float tuftLen = 0.038f + 0.014f * hash01(h);
                const float coilPhase = hash01(h + 5) * kTwoPi;
                std::vector<Vec3> spine, outs;
                spine.reserve(kSegs + 1);
                outs.reserve(kSegs + 1);
                for (int s = 0; s <= kSegs; ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(kSegs);
                    const float coilR = 0.008f * std::sin(t * kPi);
                    const float a = coilPhase + t * kTwoPi * 1.5f;
                    const Vec3 p = root + radialOut * (tuftLen * t)
                                 + (tangentA * std::cos(a) + tangentB * std::sin(a)) * coilR;
                    spine.push_back(p);
                    outs.push_back((tangentA * std::cos(a + 0.5f) + tangentB * std::sin(a + 0.5f) + radialOut * 0.4f)
                                       .normalizedOr(radialOut));
                }
                RibbonParams rp{};
                rp.rootWidth = 0.018f;
                rp.midWidth = 0.022f;
                rp.tipWidth = 0.006f;
                rp.thickness = 0.0020f;
                rp.archHeight = 0.0036f;
                rp.variant = nextVariant();
                appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, &report);
            }
            break;
        }

        case 9: { /* MOHAWK: bold midline crest of spiked/tapered layered cards */
            constexpr int kSpikes = 22;
            for (int i = 0; i < kSpikes; ++i) {
                const uint32_t h = static_cast<uint32_t>(i * 61 + 887);
                const float u = static_cast<float>(i) / static_cast<float>(kSpikes - 1);
                /* Arc along the sagittal midline from brow hairline to nape. */
                const float elev = lerpf(0.55f, -0.15f, u);
                const float zSign = (u < 0.35f) ? -1.0f : 1.0f;
                const float arcAngle = lerpf(-0.48f, 1.85f, u);
                const Vec3 dir(0.06f * hashSigned(h), std::cos(arcAngle), std::sin(arcAngle));
                (void)elev; (void)zSign;
                const Vec3 root = anchorOnScalp(field, headCenter, dir.normalizedOr(Vec3(0, 1, 0)), 0.008f);
                const Vec3 outN = collisionNormal(field, root);
                const float spikeLen = headH * (0.28f + 0.14f * std::sin(u * kPi)) * (0.85f + 0.25f * hash01(h + 2));

                constexpr int kSegs = 5;
                std::vector<Vec3> spine, outs;
                spine.reserve(kSegs + 1);
                outs.reserve(kSegs + 1);
                for (int s = 0; s <= kSegs; ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(kSegs);
                    const Vec3 p = root + outN * (spikeLen * t)
                                 + Vec3(0.004f * hashSigned(h + 4) * t, 0.008f * t, -0.012f * t * t);
                    spine.push_back(p);
                    outs.push_back(Vec3((i & 1) ? 1.0f : -1.0f, 0.2f, 0.1f).normalizedOr(Vec3(1, 0, 0)));
                }
                RibbonParams rp{};
                rp.rootWidth = 0.022f;
                rp.midWidth = 0.018f;
                rp.tipWidth = 0.003f;
                rp.thickness = 0.0020f;
                rp.archHeight = 0.0030f;
                rp.variant = nextVariant();
                appendWatertightRibbon(mesh, spine, outs, rp, floorShiftY, &report);
            }
            break;
        }

        default:
            break;
    }

    return report;
}

} // namespace af
