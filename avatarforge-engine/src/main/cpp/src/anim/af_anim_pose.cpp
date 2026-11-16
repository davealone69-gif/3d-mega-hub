/*
 * af_anim_pose.cpp -- pose evaluation, bone masks, and blending (weighted, masked, additive).
 *
 * THE ALLOCATION CONTRACT
 * -----------------------
 * Pose::allocate() sizes every buffer once. resetToRest(), computeMatrices(), blendPoses(),
 * blendMasked() and blendAdditive() then only write into buffers that already exist; none of them
 * can grow one. That is not a style preference: at 60 Hz on a phone, a per-frame std::vector resize
 * inside the joint loop is a heap allocation per frame per pose, and with a crossfade plus two
 * layers that is five allocations per frame on the render thread. The host test suite proves the
 * contract by installing a counting operator new and asserting the count does not move across a
 * thousand frames after warm-up, rather than taking the comment's word for it.
 *
 * BLENDING CHOICES, STATED SO THEY CAN BE ARGUED WITH
 * ---------------------------------------------------
 * Rotations blend with slerp (af::Quat::slerp), which already takes the shortest arc and falls back
 * to a normalised lerp above cos(0.5 degrees) -- so the small angular differences a crossfade
 * actually produces take the cheap path automatically and the expensive trigonometry only runs on
 * the large rotations where it matters. Translations and scales lerp. Morph weights lerp.
 *
 * blendPoses() normalises over the weights actually supplied, so a crossfade pair whose weights do
 * not yet sum to 1 (the first frame of a transition, or a transition interrupted mid-flight) does
 * not produce a pose that shrinks toward the rest pose.
 *
 * blendAdditive() treats the additive clip as expressed relative to the rest pose and applies
 *     out = base * (rest^-1 * additive)^weight
 * per joint, with the exponent taken as slerp(identity, delta, weight). That is what lets the
 * engine's authored "breathe" clip -- two tracks, five seconds, chest and neck -- run on top of a
 * walk cycle without the walk cycle having to know about it.
 */
#include "af_anim.h"

#include <algorithm>
#include <cmath>

namespace af {
namespace anim {

/* ====================================================================================== Pose === */

void Pose::allocate(const Skeleton& sk, int morphCount) {
    const size_t n = static_cast<size_t>(sk.count());
    /* assign() is used rather than resize() so that growing an existing pose resets it to rest
       instead of leaving stale transforms in the new slots. */
    t.assign(n, Vec3(0, 0, 0));
    r.assign(n, Quat::identity());
    s.assign(n, Vec3(1, 1, 1));
    local.assign(n, Mat4::identity());
    world.assign(n, Mat4::identity());
    skin.assign(n, Mat4::identity());
    morphs.assign(static_cast<size_t>(morphCount > 0 ? morphCount : 0), 0.0f);
    rootMotion = Vec3(0, 0, 0);
    matricesValid = false;
    resetToRest(sk);
}

void Pose::resetToRest(const Skeleton& sk) {
    const size_t n = static_cast<size_t>(sk.count());
    if (t.size() != n) {
        /* Wrong size means allocate() was never called for this skeleton. Sizing here keeps the
           pose usable; the caller still owns the one-time cost. */
        t.resize(n, Vec3(0, 0, 0));
        r.resize(n, Quat::identity());
        s.resize(n, Vec3(1, 1, 1));
        local.resize(n, Mat4::identity());
        world.resize(n, Mat4::identity());
        skin.resize(n, Mat4::identity());
    }
    for (size_t i = 0; i < n; ++i) {
        t[i] = sk.restT[i];
        r[i] = sk.restR[i];
        s[i] = sk.restS[i];
    }
    std::fill(morphs.begin(), morphs.end(), 0.0f);
    rootMotion = Vec3(0, 0, 0);
    matricesValid = false;
}

void Pose::computeMatrices(const Skeleton& sk) {
    const int n = sk.count();
    if (static_cast<int>(t.size()) != n) return;
    /* sk.order is parents-before-children, validated at construction, so world[p] is already final
       when world[i] reads it. Iterating 0..n instead would depend on the file's node ordering being
       topological, which glTF does not promise. */
    for (int idx = 0; idx < static_cast<int>(sk.order.size()); ++idx) {
        const int i = sk.order[static_cast<size_t>(idx)];
        const size_t j = static_cast<size_t>(i);
        local[j] = Mat4::trs(t[j], r[j], s[j]);
        const int p = sk.parents[j];
        world[j] = (p >= 0 && p < n) ? (world[static_cast<size_t>(p)] * local[j]) : local[j];
    }
    if (sk.hasInverseBind) {
        for (int i = 0; i < n; ++i) {
            const size_t j = static_cast<size_t>(i);
            skin[j] = sk.meshNodeInverse * world[j] * sk.inverseBind[j];
        }
    }
    matricesValid = true;
}

bool Pose::isFinite() const {
    for (const Vec3& v : t) if (!v.isFinite()) return false;
    for (const Vec3& v : s) if (!v.isFinite()) return false;
    for (const Quat& q : r) {
        if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w)) return false;
    }
    for (float w : morphs) if (!std::isfinite(w)) return false;
    return true;
}

/* ================================================================================= BoneMask ==== */

void BoneMask::allocate(const Skeleton& sk) {
    include.assign(static_cast<size_t>(sk.count()), 0);
}

void BoneMask::setAll(bool value) {
    std::fill(include.begin(), include.end(), static_cast<uint8_t>(value ? 1 : 0));
}

void BoneMask::set(int joint, bool value) {
    if (joint < 0 || joint >= static_cast<int>(include.size())) return;
    include[static_cast<size_t>(joint)] = static_cast<uint8_t>(value ? 1 : 0);
}

bool BoneMask::test(int joint) const {
    if (joint < 0 || joint >= static_cast<int>(include.size())) return false;
    return include[static_cast<size_t>(joint)] != 0;
}

int BoneMask::includedCount() const {
    int n = 0;
    for (uint8_t v : include) if (v) ++n;
    return n;
}

namespace {

/** Sets `joint` and, when requested, everything below it. */
void includeSubtree(const Skeleton& sk, int joint, bool descendants, BoneMask* mask) {
    if (joint < 0 || joint >= sk.count()) return;
    mask->set(joint, true);
    if (!descendants) return;
    /* Iterative breadth-first over the parent array: the skeleton has no child lists, and building
       one per call would allocate inside a function the caller may use per frame. */
    std::vector<int> frontier;
    frontier.push_back(joint);
    while (!frontier.empty()) {
        const int parent = frontier.back();
        frontier.pop_back();
        for (int i = 0; i < sk.count(); ++i) {
            if (sk.parents[static_cast<size_t>(i)] != parent) continue;
            if (mask->test(i)) continue;
            mask->set(i, true);
            frontier.push_back(i);
        }
    }
}

} /* namespace */

BoneMask BoneMask::fromRole(const Skeleton& sk, Role root, bool includeDescendants) {
    BoneMask mask;
    mask.allocate(sk);
    includeSubtree(sk, sk.jointForRole(root), includeDescendants, &mask);
    return mask;
}

BoneMask BoneMask::upperBody(const Skeleton& sk) {
    BoneMask mask;
    mask.allocate(sk);
    const int chest = sk.jointForRole(Role::Chest);
    if (chest < 0) {
        /* No Chest role means the split cannot be located, and guessing would put the mask somewhere
           the user did not ask for. Fall back to the whole body, which is at least predictable. */
        mask.setAll(true);
        return mask;
    }
    includeSubtree(sk, chest, true, &mask);
    /* The head and neck hang off the chest in a humanoid chain, so they arrive with the subtree;
       add them explicitly for rigs where the neck parents to the spine instead. */
    includeSubtree(sk, sk.jointForRole(Role::Neck), true, &mask);
    includeSubtree(sk, sk.jointForRole(Role::Head), true, &mask);
    includeSubtree(sk, sk.jointForRole(Role::LeftUpperArm), true, &mask);
    includeSubtree(sk, sk.jointForRole(Role::RightUpperArm), true, &mask);
    /* Arms parented below the chest are already in; the explicit calls cover rigs that parent the
       clavicle to the spine. Hips stay OUT of an upper-body mask, so locomotion keeps driving them. */
    return mask;
}

BoneMask BoneMask::lowerBody(const Skeleton& sk) {
    BoneMask mask;
    mask.allocate(sk);
    const int hips = sk.jointForRole(Role::Hips);
    if (hips < 0) {
        mask.setAll(true);
        return mask;
    }
    /* Hips plus both leg chains -- and deliberately NOT the whole hips subtree, which on a real
       skeleton would include the spine, the arms and the head. */
    mask.set(hips, true);
    for (int joint : sk.jointsForRole(Role::Hips)) mask.set(joint, true);
    includeSubtree(sk, sk.jointForRole(Role::LeftUpperLeg), true, &mask);
    includeSubtree(sk, sk.jointForRole(Role::RightUpperLeg), true, &mask);
    return mask;
}

/* ================================================================================== blending === */

void blendPoses(const std::vector<BlendEntry>& entries, Pose* out) {
    if (!out) return;
    float total = 0.0f;
    for (const BlendEntry& e : entries) {
        if (!e.pose) continue;
        if (!(e.weight > 0.0f)) continue;
        if (e.pose->jointCount() != out->jointCount()) continue;
        total += e.weight;
    }
    if (total <= 0.0f) return;              /* nothing to blend: leave `out` as the caller set it */
    const float inv = 1.0f / total;

    const int n = out->jointCount();
    /* Accumulators are thread_local and sized by the joint count, which does not change between
       frames, so after the first call they reuse their capacity and the steady-state allocation
       count is zero -- which is what the host test measures rather than what this comment claims. */
    thread_local std::vector<Vec3> tAcc;
    thread_local std::vector<Quat> qAcc;
    thread_local std::vector<Vec3> sAcc;
    tAcc.assign(static_cast<size_t>(n), Vec3(0, 0, 0));
    qAcc.assign(static_cast<size_t>(n), Quat(0, 0, 0, 0));
    sAcc.assign(static_cast<size_t>(n), Vec3(0, 0, 0));

    bool first = true;
    for (const BlendEntry& e : entries) {
        if (!e.pose || !(e.weight > 0.0f) || e.pose->jointCount() != n) continue;
        const float w = e.weight * inv;
        for (int i = 0; i < n; ++i) {
            const size_t j = static_cast<size_t>(i);
            tAcc[j] = tAcc[j] + e.pose->t[j] * w;
            sAcc[j] = sAcc[j] + e.pose->s[j] * w;
            if (first) {
                qAcc[j] = e.pose->r[j] * w;
            } else {
                /* Flip to the same hemisphere before accumulating: summing quaternions on opposite
                   sides of the sphere cancels toward zero, which normalises to an arbitrary
                   rotation. This is the standard weighted-nlerp fix. */
                const Quat& b = e.pose->r[j];
                const float d = qAcc[j].dot(b);
                qAcc[j] = (d < 0.0f) ? (qAcc[j] + (b * -w)) : (qAcc[j] + b * w);
            }
        }
        first = false;
    }
    for (int i = 0; i < n; ++i) {
        const size_t j = static_cast<size_t>(i);
        out->t[j] = tAcc[j];
        out->s[j] = sAcc[j];
        const Quat q = qAcc[j];
        out->r[j] = (std::fabs(q.dot(q)) < 1e-12f) ? out->r[j] : q.normalized();
    }
    out->matricesValid = false;
}

void blendMasked(const Pose& base, const Pose& overlay, float weight, const BoneMask& mask,
                 Pose* out) {
    if (!out) return;
    const int n = out->jointCount();
    if (base.jointCount() != n) return;
    const float w = saturate(weight);
    const bool overlayUsable = (overlay.jointCount() == n);
    for (int i = 0; i < n; ++i) {
        const size_t j = static_cast<size_t>(i);
        out->t[j] = base.t[j];
        out->r[j] = base.r[j];
        out->s[j] = base.s[j];
        if (!overlayUsable || !mask.test(i) || w <= 0.0f) continue;
        out->t[j] = base.t[j].lerp(overlay.t[j], w);
        out->s[j] = base.s[j].lerp(overlay.s[j], w);
        out->r[j] = Quat::slerp(base.r[j], overlay.r[j], w);
    }
    /* Morphs are not maskable: a bone mask has nothing to say about a face target. The overlay's
       morphs win in proportion to `weight`, which is what a facial layer over a body layer means. */
    if (overlayUsable && overlay.morphs.size() == out->morphs.size()) {
        for (size_t m = 0; m < out->morphs.size(); ++m) {
            out->morphs[m] = lerpf(base.morphs.size() > m ? base.morphs[m] : 0.0f, overlay.morphs[m], w);
        }
    } else {
        for (size_t m = 0; m < out->morphs.size() && m < base.morphs.size(); ++m) out->morphs[m] = base.morphs[m];
    }
    out->matricesValid = false;
}

void blendAdditive(const Skeleton& sk, const Pose& base, const Pose& additive, float weight,
                   const BoneMask& mask, Pose* out) {
    if (!out) return;
    const int n = out->jointCount();
    if (base.jointCount() != n || additive.jointCount() != n || sk.count() != n) return;
    const float w = saturate(weight);
    for (int i = 0; i < n; ++i) {
        const size_t j = static_cast<size_t>(i);
        out->t[j] = base.t[j];
        out->r[j] = base.r[j];
        out->s[j] = base.s[j];
        if (!mask.test(i) || w <= 0.0f) continue;
        /* delta = rest^-1 * additive, then out = base * delta^w. */
        const Quat delta = sk.restR[j].conjugate() * additive.r[j];
        const Quat scaled = Quat::slerp(Quat::identity(), delta.normalized(), w);
        out->r[j] = (base.r[j] * scaled).normalized();
        const Vec3 tDelta = additive.t[j] - sk.restT[j];
        out->t[j] = base.t[j] + tDelta * w;
        const Vec3 sDelta(additive.s[j].x / (std::fabs(sk.restS[j].x) < kEps ? 1.0f : sk.restS[j].x),
                          additive.s[j].y / (std::fabs(sk.restS[j].y) < kEps ? 1.0f : sk.restS[j].y),
                          additive.s[j].z / (std::fabs(sk.restS[j].z) < kEps ? 1.0f : sk.restS[j].z));
        out->s[j] = Vec3(lerpf(1.0f, sDelta.x, w) * base.s[j].x,
                         lerpf(1.0f, sDelta.y, w) * base.s[j].y,
                         lerpf(1.0f, sDelta.z, w) * base.s[j].z);
    }
    for (size_t m = 0; m < out->morphs.size(); ++m) {
        const float b = (m < base.morphs.size()) ? base.morphs[m] : 0.0f;
        const float a = (m < additive.morphs.size()) ? additive.morphs[m] : 0.0f;
        out->morphs[m] = saturate(b + a * w);
    }
    out->matricesValid = false;
}

void blendMorphWeights(const std::vector<float>& a, const std::vector<float>& b, float w,
                       std::vector<float>* out) {
    if (!out) return;
    const float t = saturate(w);
    const size_t n = std::max(a.size(), b.size());
    out->resize(n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const float av = (i < a.size()) ? a[i] : 0.0f;
        const float bv = (i < b.size()) ? b[i] : 0.0f;
        (*out)[i] = lerpf(av, bv, t);
    }
}

void blendMorphWeightEntries(const std::vector<BlendEntry>& entries,
                             const std::vector<const std::vector<float>*>& arrays,
                             std::vector<float>* out) {
    if (!out) return;
    float total = 0.0f;
    const size_t m = std::min(entries.size(), arrays.size());
    for (size_t i = 0; i < m; ++i) {
        if (!arrays[i] || !(entries[i].weight > 0.0f)) continue;
        total += entries[i].weight;
    }
    if (total <= 0.0f) { out->clear(); return; }
    size_t width = 0;
    for (size_t i = 0; i < m; ++i) if (arrays[i]) width = std::max(width, arrays[i]->size());
    out->assign(width, 0.0f);
    const float inv = 1.0f / total;
    for (size_t i = 0; i < m; ++i) {
        if (!arrays[i] || !(entries[i].weight > 0.0f)) continue;
        const float w = entries[i].weight * inv;
        for (size_t k = 0; k < arrays[i]->size() && k < width; ++k) (*out)[k] += (*arrays[i])[k] * w;
    }
}

} /* namespace anim */
} /* namespace af */
