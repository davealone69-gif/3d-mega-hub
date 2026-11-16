/*
 * af_anim_clip.cpp -- clip construction (from an imported glTF animation, and from the engine's own
 *                     authored library) and the keyframe sampler.
 *
 * TWO SOURCES, ONE REPRESENTATION
 * -------------------------------
 * Clip::fromGlb binds an af::ImportedAnimation to a Skeleton. Clip::fromAuthored converts the 16
 * hand-keyed clips that af_rig.cpp already ships (af::builtinClips()) into the same representation,
 * resolving each af::Track by joint NAME exactly the way af_engine.cpp's sampleClipInto does. So the
 * generated avatar's authored walk/run/jump library and a downloaded Fox.glb become the same kind of
 * object and can be blended, layered, transitioned and retargeted by one code path. Nothing here
 * duplicates the authored key data; it is converted, not re-authored.
 *
 * SAMPLING
 * --------
 * glTF's rules, implemented literally:
 *   LINEAR      nlerp for translation/scale, slerp along the shortest arc for rotation;
 *   STEP        hold the lower key;
 *   CUBICSPLINE the Hermite blend with (out-tangent of key k) and (in-tangent of key k+1) scaled by
 *               the segment length -- the layout 3DDD's AnimationSampler.kt documents and the one
 *               the glTF specification defines. Getting the tangent pairing backwards is the single
 *               most common bug in a hand-written cubic-spline sampler and it produces motion that
 *               looks almost right, so the host test asserts the two ends and the midpoint against
 *               closed-form values;
 *   SMOOTHSTEP  the authored library's easing (see Interp::Smoothstep in af_anim.h).
 *
 * Out-of-range times clamp to the first/last key, which is what glTF viewers do and what makes
 * scrubbing past the end stable. Looping is the Player's decision, never the sampler's, so that a
 * seek to t and a loop through t agree on what the pose at t is.
 */
#include "af_anim.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace af {
namespace anim {

/* ================================================================================= categories = */

const char* categoryName(Category c) {
    switch (c) {
        case Category::Uncategorized: return "Uncategorized";
        case Category::Idle: return "Idle";
        case Category::Walking: return "Walking";
        case Category::Running: return "Running";
        case Category::Jumping: return "Jumping";
        case Category::Dancing: return "Dancing";
        case Category::Combat: return "Combat";
        case Category::Gestures: return "Gestures";
        case Category::Emotions: return "Emotions";
        case Category::Talking: return "Talking";
        case Category::Facial: return "Facial expressions";
        case Category::COUNT: return "<all>";
    }
    return "<invalid category>";
}

namespace {

/** Lowercase, alphanumerics only, so "Turn 360" and "turn_360" both match. */
std::string squash(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out.push_back(c);
        else if (c >= 'A' && c <= 'Z') out.push_back(static_cast<char>(c - 'A' + 'a'));
    }
    return out;
}

bool contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} /* namespace */

Category categoryForClipName(const std::string& name) {
    const std::string n = squash(name);
    if (n.empty()) return Category::Uncategorized;
    /* Order matters: "run" is a substring of nothing dangerous, but "jog" must be checked before
       the generic locomotion terms, and "idle" variants before "walk" so that "idle_walk_blend"
       stays an idle. */
    if (contains(n, "jump") || contains(n, "hop") || contains(n, "leap") || contains(n, "fall") ||
        contains(n, "land")) return Category::Jumping;
    if (contains(n, "run") || contains(n, "sprint") || contains(n, "dash") || contains(n, "jog")) {
        return Category::Running;
    }
    if (contains(n, "walk") || contains(n, "stroll") || contains(n, "pace") || contains(n, "step")) {
        return Category::Walking;
    }
    if (contains(n, "dance") || contains(n, "hiphop") || contains(n, "salsa") || contains(n, "waltz") ||
        contains(n, "macarena") || contains(n, "twist")) return Category::Dancing;
    if (contains(n, "punch") || contains(n, "kick") || contains(n, "jab") || contains(n, "hit") ||
        contains(n, "attack") || contains(n, "block") || contains(n, "dodge") || contains(n, "pushup") ||
        contains(n, "squat") || contains(n, "combat") || contains(n, "slash") || contains(n, "shoot")) {
        return Category::Combat;
    }
    if (contains(n, "wave") || contains(n, "point") || contains(n, "nod") || contains(n, "shake") ||
        contains(n, "shrug") || contains(n, "clap") || contains(n, "beckon") || contains(n, "salute") ||
        contains(n, "turn") || contains(n, "gesture")) return Category::Gestures;
    if (contains(n, "talk") || contains(n, "speak") || contains(n, "chat") || contains(n, "viseme") ||
        contains(n, "lipsync") || contains(n, "mouth")) return Category::Talking;
    if (contains(n, "happy") || contains(n, "sad") || contains(n, "angry") || contains(n, "anger") ||
        contains(n, "surprise") || contains(n, "fear") || contains(n, "disgust") || contains(n, "smile") ||
        contains(n, "laugh") || contains(n, "cry") || contains(n, "emotion") || contains(n, "expression")) {
        return Category::Emotions;
    }
    if (contains(n, "blink") || contains(n, "brow") || contains(n, "eye") || contains(n, "jaw") ||
        contains(n, "face") || contains(n, "morph") || contains(n, "blendshape")) return Category::Facial;
    if (contains(n, "idle") || contains(n, "survey") || contains(n, "stand") || contains(n, "breathe") ||
        contains(n, "rest") || contains(n, "tpose") || contains(n, "apose") || contains(n, "sit") ||
        contains(n, "neutral") || contains(n, "still")) return Category::Idle;
    return Category::Uncategorized;
}

/* ==================================================================================== sampling = */

void SampleCache::bind(const Clip& clip) {
    cursor.assign(clip.channels.size(), 0u);
}

void SampleCache::reset() {
    std::fill(cursor.begin(), cursor.end(), 0u);
}

int findKeyInterval(const std::vector<float>& times, float t, uint32_t* cursor) {
    const int n = static_cast<int>(times.size());
    if (n < 2) return 0;
    int lo = 0;
    if (cursor && *cursor < static_cast<uint32_t>(n - 1)) lo = static_cast<int>(*cursor);
    /* Forward from the previous frame's key first: a 60 Hz timeline advances by a fraction of a
       key interval per frame, so this is normally zero or one step. */
    while (lo + 1 < n - 1 && times[static_cast<size_t>(lo) + 1] <= t) ++lo;
    /* Then back off if the caller seeked backwards, which scrubbing does constantly. */
    while (lo > 0 && times[static_cast<size_t>(lo)] > t) --lo;
    if (cursor) *cursor = static_cast<uint32_t>(lo);
    return lo;
}

namespace {

/** Component count of a channel's stored samples. */
int channelStride(const Channel& ch) {
    return (ch.interp == Interp::CubicSpline) ? 3 : 1;
}

/* A CUBICSPLINE key stores three sets per keyframe in glTF's order: in-tangent, value, out-tangent.
   The samplers below ask for them by meaning instead -- slot 0 = value, 1 = in-tangent,
   2 = out-tangent -- so that a caller cannot accidentally read an in-tangent as a position. This
   table is the single place that knows the file's order; getting it wrong is invisible in a still
   frame and shows up as a spline that overshoots the wrong way. */
size_t cubicSlot(int slot) {
    static const size_t kGltfOrder[3] = {1u, 0u, 2u};   /* value, in-tangent, out-tangent */
    if (slot < 0 || slot > 2) return 1u;
    return kGltfOrder[slot];
}

Vec4 trsValueAt(const Channel& ch, size_t key, int slot) {
    /* For Linear/Step the three meanings are stored once, so every slot reads the same entry. */
    const size_t base = (ch.interp == Interp::CubicSpline) ? (key * 3 + cubicSlot(slot)) : key;
    if (base >= ch.values.size()) return Vec4(0, 0, 0, 0);
    return ch.values[base];
}

/* Hermite smoothstep on the unit interval: 0 at u=0, 1 at u=1, zero derivative at both ends. The
   authored library (af_rig.cpp) eases every key this way, so a wrong polynomial here is not a
   cosmetic slip -- it changes the feel of all 15 built-in clips. The host test pins the value at
   u=0.25 to 0.15625 exactly, which is what caught the (3 - 2) form that silently reduced this to
   u squared. */
float smoothstepUnit(float u) { return u * u * (3.0f - 2.0f * u); }

} /* namespace */

void sampleChannelVec(const Channel& ch, float t, uint32_t* cursor, Vec4* out) {
    if (!out) return;
    const size_t keys = ch.times.size();
    if (keys == 0 || ch.values.empty()) { *out = Vec4(0, 0, 0, 0); return; }
    if (keys == 1 || t <= ch.times.front()) { *out = trsValueAt(ch, 0, 0); return; }
    if (t >= ch.times.back()) { *out = trsValueAt(ch, keys - 1, 0); return; }

    const int low = findKeyInterval(ch.times, t, cursor);
    const int high = low + 1;
    if (high >= static_cast<int>(keys)) { *out = trsValueAt(ch, keys - 1, 0); return; }

    if (ch.interp == Interp::Step) { *out = trsValueAt(ch, static_cast<size_t>(low), 0); return; }

    const float t0 = ch.times[static_cast<size_t>(low)];
    const float t1 = ch.times[static_cast<size_t>(high)];
    const float span = t1 - t0;
    if (span <= kEps) { *out = trsValueAt(ch, static_cast<size_t>(low), 0); return; }
    float u = clampf((t - t0) / span, 0.0f, 1.0f);

    const Vec4 p0 = trsValueAt(ch, static_cast<size_t>(low), 0);
    const Vec4 p1 = trsValueAt(ch, static_cast<size_t>(high), 0);

    if (ch.interp == Interp::CubicSpline) {
        /* glTF: p(u) = (2u^3-3u^2+1)p0 + (u^3-2u^2+u)*dt*m0 + (-2u^3+3u^2)p1 + (u^3-u^2)*dt*m1
           with m0 = OUT-tangent of the lower key and m1 = IN-tangent of the upper key. */
        const Vec4 m0 = trsValueAt(ch, static_cast<size_t>(low), 2) * span;
        const Vec4 m1 = trsValueAt(ch, static_cast<size_t>(high), 1) * span;
        const float u2 = u * u;
        const float u3 = u2 * u;
        const float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
        const float h10 = u3 - 2.0f * u2 + u;
        const float h01 = -2.0f * u3 + 3.0f * u2;
        const float h11 = u3 - u2;
        *out = p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11;
        return;
    }

    if (ch.interp == Interp::Smoothstep) u = smoothstepUnit(u);
    if (ch.path == Path::Rotation) {
        const Quat a(p0.x, p0.y, p0.z, p0.w);
        const Quat b(p1.x, p1.y, p1.z, p1.w);
        const Quat q = Quat::slerp(a, b, u);
        *out = Vec4(q.x, q.y, q.z, q.w);
        return;
    }
    *out = Vec4(lerpf(p0.x, p1.x, u), lerpf(p0.y, p1.y, u),
                lerpf(p0.z, p1.z, u), lerpf(p0.w, p1.w, u));
}

namespace {

/** Samples a morph-weight channel at `t` into `out` (componentCount entries appended). */
void sampleWeightChannel(const Channel& ch, float t, uint32_t* cursor, std::vector<float>* out) {
    const size_t keys = ch.times.size();
    const int comps = ch.components;
    if (keys == 0 || comps <= 0 || ch.weightValues.empty()) return;
    const size_t stride = static_cast<size_t>(comps) * static_cast<size_t>(channelStride(ch));
    auto keyAt = [&](size_t key, int slot) -> const float* {
        /* Same slot meaning as trsValueAt: 0 = value, 1 = in-tangent, 2 = out-tangent. */
        const size_t base = (ch.interp == Interp::CubicSpline)
                                ? (key * static_cast<size_t>(comps) * 3 + cubicSlot(slot) * static_cast<size_t>(comps))
                                : (key * static_cast<size_t>(comps));
        if (base + static_cast<size_t>(comps) > ch.weightValues.size()) return nullptr;
        return &ch.weightValues[base];
    };
    out->assign(static_cast<size_t>(comps), 0.0f);

    if (keys == 1 || t <= ch.times.front()) {
        const float* v = keyAt(0, 0);
        if (v) for (int c = 0; c < comps; ++c) (*out)[static_cast<size_t>(c)] = v[c];
        return;
    }
    if (t >= ch.times.back()) {
        const float* v = keyAt(keys - 1, 0);
        if (v) for (int c = 0; c < comps; ++c) (*out)[static_cast<size_t>(c)] = v[c];
        return;
    }
    const int low = findKeyInterval(ch.times, t, cursor);
    const int high = low + 1;
    if (high >= static_cast<int>(keys)) return;
    if (ch.interp == Interp::Step) {
        const float* v = keyAt(static_cast<size_t>(low), 0);
        if (v) for (int c = 0; c < comps; ++c) (*out)[static_cast<size_t>(c)] = v[c];
        return;
    }
    const float span = ch.times[static_cast<size_t>(high)] - ch.times[static_cast<size_t>(low)];
    if (span <= kEps) return;
    float u = clampf((t - ch.times[static_cast<size_t>(low)]) / span, 0.0f, 1.0f);
    const float* p0 = keyAt(static_cast<size_t>(low), 0);
    const float* p1 = keyAt(static_cast<size_t>(high), 0);
    if (!p0 || !p1) return;
    if (ch.interp == Interp::CubicSpline) {
        const float* m0 = keyAt(static_cast<size_t>(low), 2);
        const float* m1 = keyAt(static_cast<size_t>(high), 1);
        if (!m0 || !m1) return;
        const float u2 = u * u, u3 = u2 * u;
        const float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
        const float h10 = (u3 - 2.0f * u2 + u) * span;
        const float h01 = -2.0f * u3 + 3.0f * u2;
        const float h11 = (u3 - u2) * span;
        for (int c = 0; c < comps; ++c) {
            (*out)[static_cast<size_t>(c)] = h00 * p0[c] + h10 * m0[c] + h01 * p1[c] + h11 * m1[c];
        }
        return;
    }
    if (ch.interp == Interp::Smoothstep) u = smoothstepUnit(u);
    for (int c = 0; c < comps; ++c) {
        (*out)[static_cast<size_t>(c)] = lerpf(p0[c], p1[c], u);
    }
    (void)stride;
}

} /* namespace */

void sampleClip(const Clip& clip, const Skeleton& sk, float time, Pose* out, SampleCache* cache) {
    if (!out) return;
    out->resetToRest(sk);
    const float t = clip.duration > 0.0f ? clampf(time, 0.0f, clip.duration) : 0.0f;

    /* A per-call scratch buffer for weight channels would allocate every frame. It is thread_local
       and only grows, so the steady-state cost is zero -- and it stays out of Pose because Pose is
       the caller's data and may be shared across players. */
    thread_local std::vector<float> weightScratch;

    for (size_t ci = 0; ci < clip.channels.size(); ++ci) {
        const Channel& ch = clip.channels[ci];
        uint32_t* cursor = (cache && ci < cache->cursor.size()) ? &cache->cursor[ci] : nullptr;
        if (ch.path == Path::Weights) {
            sampleWeightChannel(ch, t, cursor, &weightScratch);
            for (size_t c = 0; c < weightScratch.size(); ++c) {
                const size_t idx = static_cast<size_t>(ch.morphOffset) + c;
                if (idx < out->morphs.size()) out->morphs[idx] = weightScratch[c];
            }
            continue;
        }
        if (ch.joint < 0 || ch.joint >= out->jointCount()) continue;
        Vec4 v;
        sampleChannelVec(ch, t, cursor, &v);
        const size_t j = static_cast<size_t>(ch.joint);
        switch (ch.path) {
            case Path::Translation:
                out->t[j] = Vec3(v.x, v.y, v.z);
                break;
            case Path::Rotation: {
                const Quat q(v.x, v.y, v.z, v.w);
                /* A zero-length quaternion in a file would poison every descendant through the
                   world pass. Normalising a near-zero one cannot recover a rotation, so hold rest. */
                out->r[j] = (std::fabs(q.dot(q)) < 1e-8f) ? out->r[j] : q.normalized();
                break;
            }
            case Path::Scale: {
                Vec3 s(v.x, v.y, v.z);
                if (std::fabs(s.x) < kEps) s.x = kEps;
                if (std::fabs(s.y) < kEps) s.y = kEps;
                if (std::fabs(s.z) < kEps) s.z = kEps;
                out->s[j] = s;
                break;
            }
            case Path::Weights:
                break;
        }
    }
    out->matricesValid = false;
}

/* ============================================================================= Clip::fromGlb === */

namespace {

/**
 * Morph-target base offset for a node: glTF weights channels are per-primitive, and this Player
 * keeps one flat morph array per asset, so a channel targeting the second mesh must be offset past
 * the first mesh's targets. Returns -1 when the node drives no mesh with morph targets.
 */
int morphBaseOffsetForNode(const GlbAsset& asset, int node, int* outCount) {
    if (outCount) *outCount = 0;
    if (node < 0 || node >= static_cast<int>(asset.nodes.size())) return -1;
    const int mesh = asset.nodes[static_cast<size_t>(node)].mesh;
    if (mesh < 0 || mesh >= static_cast<int>(asset.meshes.size())) return -1;
    const int count = asset.meshes[static_cast<size_t>(mesh)].morphTargetCount;
    if (count <= 0) return -1;
    int offset = 0;
    for (int m = 0; m < mesh; ++m) offset += asset.meshes[static_cast<size_t>(m)].morphTargetCount;
    if (outCount) *outCount = count;
    return offset;
}

int totalMorphTargets(const GlbAsset& asset) {
    int total = 0;
    for (const auto& m : asset.meshes) total += m.morphTargetCount;
    return total;
}

} /* namespace */

bool Clip::fromGlb(const GlbAsset& asset, int animationIndex, const Skeleton& sk,
                   Clip* out, std::vector<std::string>* skipped, std::string* error) {
    auto fail = [&](const char* msg) { if (error) *error = msg; return false; };
    if (!out) return fail("Clip::fromGlb: null output");
    *out = Clip();
    if (animationIndex < 0 || animationIndex >= static_cast<int>(asset.animations.size())) {
        return fail("animation index is outside the imported document's animations");
    }
    const ImportedAnimation& anim = asset.animations[static_cast<size_t>(animationIndex)];

    /* node index -> skeleton index, from the closure the skeleton recorded */
    std::unordered_map<int, int> nodeToJoint;
    nodeToJoint.reserve(sk.sourceNode.size() * 2 + 1);
    for (size_t i = 0; i < sk.sourceNode.size(); ++i) {
        if (sk.sourceNode[i] >= 0) nodeToJoint[sk.sourceNode[i]] = static_cast<int>(i);
    }

    out->name = anim.name.empty() ? ("animation_" + std::to_string(animationIndex)) : anim.name;
    out->sourceAnimation = animationIndex;
    out->authored = false;
    /* glTF carries no loop flag: a file cannot say whether its walk cycle is meant to loop. Every
       imported clip therefore defaults to looping and the UI exposes the switch, rather than the
       importer inventing a value the file did not contain. */
    out->looping = true;
    out->category = categoryForClipName(out->name);
    out->duration = anim.duration;

    for (const ImportedAnimationChannel& src : anim.channels) {
        Channel ch;
        ch.interp = (src.interpolation == 1) ? Interp::Step
                    : (src.interpolation == 2) ? Interp::CubicSpline
                                               : Interp::Linear;
        ch.times = src.times;
        if (ch.times.empty()) {
            if (skipped) skipped->push_back(out->name + ": a channel has no keyframe times");
            continue;
        }

        if (src.path == 3) {
            int morphCount = 0;
            const int offset = morphBaseOffsetForNode(asset, src.node, &morphCount);
            if (offset < 0 || morphCount <= 0) {
                if (skipped) skipped->push_back(out->name + ": a weights channel targets node " +
                                                std::to_string(src.node) +
                                                ", which has no morph targets");
                continue;
            }
            if (src.componentCount <= 0 || src.weightValues.empty()) {
                if (skipped) skipped->push_back(out->name + ": a weights channel has no sample data");
                continue;
            }
            ch.path = Path::Weights;
            ch.joint = -1;
            ch.morphOffset = offset;
            /* A channel may address fewer targets than the mesh has (legal, if unusual); take the
               smaller of the two so sampling cannot write past the mesh's own targets. */
            ch.morphCount = std::min(morphCount, src.componentCount);
            ch.components = src.componentCount;
            if (ch.interp == Interp::CubicSpline && !src.weightInTangents.empty() &&
                !src.weightOutTangents.empty()) {
                /* Re-interleave into the (in, value, out) per-key order the sampler indexes, matching
                   what Clip::fromGlb does for the TRS paths. */
                const size_t comps = static_cast<size_t>(src.componentCount);
                const size_t keys = std::min(src.weightValues.size() / comps,
                                             std::min(src.weightInTangents.size() / comps,
                                                      src.weightOutTangents.size() / comps));
                ch.weightValues.clear();
                ch.weightValues.reserve(keys * comps * 3);
                for (size_t k = 0; k < keys; ++k) {
                    ch.weightValues.insert(ch.weightValues.end(),
                                           src.weightInTangents.begin() + static_cast<ptrdiff_t>(k * comps),
                                           src.weightInTangents.begin() + static_cast<ptrdiff_t>((k + 1) * comps));
                    ch.weightValues.insert(ch.weightValues.end(),
                                           src.weightValues.begin() + static_cast<ptrdiff_t>(k * comps),
                                           src.weightValues.begin() + static_cast<ptrdiff_t>((k + 1) * comps));
                    ch.weightValues.insert(ch.weightValues.end(),
                                           src.weightOutTangents.begin() + static_cast<ptrdiff_t>(k * comps),
                                           src.weightOutTangents.begin() + static_cast<ptrdiff_t>((k + 1) * comps));
                }
            } else {
                ch.weightValues = src.weightValues;
            }
            out->channels.push_back(std::move(ch));
            continue;
        }

        const auto it = nodeToJoint.find(src.node);
        if (it == nodeToJoint.end()) {
            /* The channel animates a node that is not part of this skeleton. Reported, not
               silently dropped: this is how a viewer ends up claiming to play a clip that is
               visibly doing nothing. */
            if (skipped) {
                const std::string nodeName = (src.node >= 0 && src.node < static_cast<int>(asset.nodes.size()))
                                                 ? asset.nodes[static_cast<size_t>(src.node)].name
                                                 : ("<node " + std::to_string(src.node) + ">");
                skipped->push_back(out->name + ": channel targets '" + nodeName +
                                   "', which is not a joint of this skeleton");
            }
            continue;
        }
        ch.joint = it->second;
        ch.path = (src.path == 0) ? Path::Translation : (src.path == 1) ? Path::Rotation : Path::Scale;
        ch.components = (ch.path == Path::Rotation) ? 4 : 3;
        ch.values = src.values;
        if (ch.interp == Interp::CubicSpline && !src.inTangents.empty() && !src.outTangents.empty()) {
            /* Re-interleave the importer's split tangent arrays back into glTF's
               (in, value, out) per-key order, which is what the sampler indexes. */
            const size_t keys = std::min(src.values.size(), std::min(src.inTangents.size(), src.outTangents.size()));
            ch.values.clear();
            ch.values.reserve(keys * 3);
            for (size_t k = 0; k < keys; ++k) {
                ch.values.push_back(src.inTangents[k]);
                ch.values.push_back(src.values[k]);
                ch.values.push_back(src.outTangents[k]);
            }
        }
        out->channels.push_back(std::move(ch));
    }

    if (out->channels.empty()) {
        return fail("the imported animation has no channel this skeleton can play");
    }
    /* Root motion is a property of the clip and the skeleton together, not of a channel: a
       translation on a finger is not travel. rootMotionJoint() picks the joint that carries the
       character, and rootTranslationDelta() measures the same joint, so the flag and the
       measurement can never disagree. */
    out->hasRootTranslation = (rootMotionJoint(sk, *out) >= 0);
    /* Duration comes from the importer, but a file whose declared duration disagrees with its own
       keys is common enough to be worth correcting rather than trusting. */
    float maxTime = 0.0f;
    for (const Channel& ch : out->channels) {
        if (!ch.times.empty()) maxTime = std::max(maxTime, ch.times.back());
    }
    if (maxTime > 0.0f) out->duration = maxTime;
    if (!(out->duration > 0.0f)) out->duration = 0.0f;
    return true;
}

/* ========================================================================== Clip::fromAuthored = */

namespace {

/* The authored library's joint vocabulary, mapped to Role explicitly.
   This is a TABLE and not a heuristic on purpose. assignRoles()/findFuzzy() exist to guess at the
   naming of FOREIGN files, and running them against the engine's own vocabulary was measured to resolve
   only 7 of the 17 roles: thigh_L, shin_L, foot_L, thigh_R, shin_R, foot_R, upperarm_01_L and
   upperarm_01_R all come back unmapped, while spine_02, chest and root collapse onto Hips together. A
   walk cycle bound that way drives the hips alone and the legs never move. Our own rig's names are known
   and fixed, so they are stated outright - and animation_system_test fails if the library grows a track
   name this table does not cover, which is the only way it can silently go stale. */
/* Two library entries are not motion: 't_pose' and 'a_pose' are single-key, 0.10 s CALIBRATION poses
   that state the generated rig's own rest shape. Transferring them onto a foreign skeleton was measured
   on mylove_rig_prepared.glb, whose bind pose is already arms-out (LeftUpperArm rest direction
   (-0.999, +0.040, 0.000)): the authored t_pose is +/-90 degrees about Y, which maps (-1,0,0) to
   (0,0,-1) and swings arms that were already out BACKWARD, collapsing a 0.825 arm span to 0.505. No
   composition of local rotations can repair that, because the mismatch is in the rest SHAPE (the
   generated rig binds arms-down), not in the rest orientation. An imported file's bind pose already is
   its own calibration, so these two are refused with a reason instead of posing the avatar wrongly. */
bool isCalibrationPose(const std::string& name) {
    return name == "t_pose" || name == "a_pose";
}

Role authoredTrackRole(const std::string& name) {
    static const struct { const char* name; Role role; } kTable[] = {
        { "root",          Role::Hips },
        { "hips",          Role::Hips },
        { "spine_02",      Role::Spine },
        { "chest",         Role::Chest },
        { "neck_01",       Role::Neck },
        { "head",          Role::Head },
        { "thigh_L",       Role::LeftUpperLeg },
        { "thigh_R",       Role::RightUpperLeg },
        { "shin_L",        Role::LeftLowerLeg },
        { "shin_R",        Role::RightLowerLeg },
        { "foot_L",        Role::LeftFoot },
        { "foot_R",        Role::RightFoot },
        { "upperarm_01_L", Role::LeftUpperArm },
        { "upperarm_01_R", Role::RightUpperArm },
        { "forearm_01_L",  Role::LeftLowerArm },
        { "forearm_01_R",  Role::RightLowerArm },
        { "hand_L",        Role::LeftHand },
        { "hand_R",        Role::RightHand },
    };
    for (const auto& e : kTable) if (name == e.name) return e.role;
    return Role::COUNT;
}

/* Every joint name the built-in library writes to, so a caller (or a test) can prove the table above
   still covers the library instead of trusting it. */
std::vector<std::string> authoredVocabulary(const std::vector<af::Clip>& library) {
    std::vector<std::string> names;
    for (const af::Clip& c : library) {
        for (const af::Track& t : c.tracks) {
            bool seen = false;
            for (const std::string& n : names) if (n == t.jointName) { seen = true; break; }
            if (!seen) names.push_back(t.jointName);
        }
    }
    return names;
}

} // namespace

std::vector<std::string> authoredTrackNames(const std::vector<af::Clip>& library) {
    return authoredVocabulary(library);
}

Role authoredRoleForTrackName(const std::string& name) {
    return authoredTrackRole(name);
}

bool authoredClipIsCalibrationPose(const std::string& clipName) {
    return isCalibrationPose(clipName);
}

bool Clip::fromAuthoredByRole(const af::Clip& authored, const Skeleton& target, Clip* out,
                              std::vector<std::string>* skipped) {
    if (!out) return false;
    *out = Clip();
    if (isCalibrationPose(authored.name)) {
        if (skipped) skipped->push_back(authored.name +
                                        ": refused - a single-key calibration pose describing the "
                                        "generated rig's rest shape; this skeleton's bind pose is its "
                                        "own calibration, and transferring it swings the arms backward");
        return false;
    }
    out->name = authored.name;
    out->duration = authored.duration;
    out->looping = authored.looping;
    out->authored = true;
    out->sourceAnimation = -1;
    out->category = categoryForClipName(authored.name);

    const int jointCount = target.count();
    if (jointCount <= 0) return false;
    /* Two authored names ("root" and "hips") both map to Role::Hips, and a target skeleton can resolve
       several roles onto one joint. One rotation channel per joint: the first track wins and the
       duplicate is reported, rather than two channels fighting over the same bone each frame. */
    std::vector<bool> rotationTaken(static_cast<size_t>(jointCount), false);

    for (const Track& tr : authored.tracks) {
        const Role role = authoredTrackRole(tr.jointName);
        if (role == Role::COUNT) {
            if (skipped) skipped->push_back(authored.name + ": track '" + tr.jointName +
                                            "' is not in the authored role table");
            continue;
        }
        const int joint = target.jointForRole(role);
        if (joint < 0) {
            if (skipped) skipped->push_back(authored.name + ": track '" + tr.jointName + "' needs role " +
                                            roleName(role) + ", which this skeleton does not have");
            continue;
        }
        if (tr.keys.empty()) continue;

        bool anyTranslation = false;
        for (const Keyframe& kf : tr.keys) {
            if (kf.translation.lengthSq() > kEps) { anyTranslation = true; break; }
        }
        if (anyTranslation && skipped) {
            skipped->push_back(authored.name + ": track '" + tr.jointName +
                               "' carries translation that is NOT transferred - the authored library is "
                               "scaled for a 1.75 m figure and an imported skeleton gives no source rig "
                               "to derive a ratio from, so applying it unscaled would overshoot");
        }

        if (rotationTaken[static_cast<size_t>(joint)]) {
            if (skipped) skipped->push_back(authored.name + ": track '" + tr.jointName +
                                            "' resolves to the same joint as an earlier track; kept the "
                                            "first");
            continue;
        }
        rotationTaken[static_cast<size_t>(joint)] = true;

        Channel rot;
        rot.joint = joint;
        rot.path = Path::Rotation;
        rot.interp = Interp::Smoothstep;
        rot.components = 4;
        rot.times.reserve(tr.keys.size());
        rot.values.reserve(tr.keys.size());
        /* REST-FRAME CONVERSION. Pose::computeMatrices() uses r[j] as a bone's FULL local rotation and
           resetToRest() fills it from sk.restR, so a clip channel replaces the rest rather than adding to
           it. The authored library was written against the generated rig, whose restRotation is identity
           for every one of its 71 joints (measured), which makes an authored key a pure deviation from
           rest. Composing that deviation on the TARGET's rest is what keeps the import's own bind pose as
           the starting point instead of overwriting it; for identity-rest skeletons such as Fox and
           mylove_rig_prepared.glb this is a no-op, and it is the difference between working and badly
           broken for a Blender-exported rig whose rest rotations are not identity.
           This does NOT fix a differing rest SHAPE: the generated rig binds arms-down while that avatar
           binds arms-out, and no composition of local rotations reconciles the two. That is why the
           calibration poses above are refused rather than converted. */
        const Quat& rest = target.restR[static_cast<size_t>(joint)];
        for (const Keyframe& kf : tr.keys) {
            rot.times.push_back(kf.time);
            const Quat q = rest * kf.rotation;
            rot.values.push_back(Vec4(q.x, q.y, q.z, q.w));
        }
        out->channels.push_back(std::move(rot));
    }
    if (out->channels.empty()) return false;
    /* No translation channel is ever emitted here, so there is no root motion to extract. Claiming
       otherwise would make consumeRootMotion() report movement the pose does not contain. */
    out->hasRootTranslation = false;
    return true;
}

bool Clip::fromAuthored(const af::Clip& authored, const Skeleton& sk, Clip* out,
                        std::vector<std::string>* skipped) {
    if (!out) return false;
    *out = Clip();
    out->name = authored.name;
    out->duration = authored.duration;
    out->looping = authored.looping;
    out->authored = true;
    out->sourceAnimation = -1;
    out->category = categoryForClipName(authored.name);

    for (const Track& tr : authored.tracks) {
        const int joint = sk.find(tr.jointName);
        const int resolved = (joint >= 0) ? joint : sk.findFuzzy(tr.jointName);
        if (resolved < 0) {
            if (skipped) skipped->push_back(authored.name + ": track '" + tr.jointName +
                                            "' has no joint in this skeleton");
            continue;
        }
        if (tr.keys.empty()) continue;

        Channel rot;
        rot.joint = resolved;
        rot.path = Path::Rotation;
        rot.interp = Interp::Smoothstep;
        rot.components = 4;
        rot.times.reserve(tr.keys.size());
        rot.values.reserve(tr.keys.size());
        bool anyTranslation = false;
        for (const Keyframe& kf : tr.keys) {
            rot.times.push_back(kf.time);
            rot.values.push_back(Vec4(kf.rotation.x, kf.rotation.y, kf.rotation.z, kf.rotation.w));
            if (kf.translation.lengthSq() > kEps) anyTranslation = true;
        }
        out->channels.push_back(std::move(rot));

        /* The authored Keyframe carries a translation that only the jump/hop style clips use; emit
           a translation channel only when some key actually moves, so a rotation-only track does
           not gain a channel that pins the joint to the origin. */
        if (anyTranslation) {
            Channel pos;
            pos.joint = resolved;
            pos.path = Path::Translation;
            pos.interp = Interp::Smoothstep;
            pos.components = 3;
            pos.times.reserve(tr.keys.size());
            pos.values.reserve(tr.keys.size());
            for (const Keyframe& kf : tr.keys) {
                pos.times.push_back(kf.time);
                pos.values.push_back(Vec4(kf.translation.x, kf.translation.y, kf.translation.z, 0.0f));
            }
            out->channels.push_back(std::move(pos));
        }
    }
    if (out->channels.empty()) return false;
    out->hasRootTranslation = (rootMotionJoint(sk, *out) >= 0);
    return true;
}

int Clip::morphChannelCount() const {
    int n = 0;
    for (const Channel& ch : channels) if (ch.path == Path::Weights) ++n;
    return n;
}

int Clip::morphSpan() const {
    int span = 0;
    for (const Channel& ch : channels) {
        if (ch.path != Path::Weights) continue;
        span = std::max(span, ch.morphOffset + ch.morphCount);
    }
    return span;
}

int totalMorphTargetsForAsset(const GlbAsset& asset) { return totalMorphTargets(asset); }

} /* namespace anim */
} /* namespace af */
