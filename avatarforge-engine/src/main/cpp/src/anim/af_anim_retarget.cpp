/*
 * af_anim_retarget.cpp -- skeleton compatibility, bone mapping and rest-pose alignment.
 *
 * WHY ROLE-BASED AND NOT NAME-BASED
 * ---------------------------------
 * The three skeletons the host tests retarget between share essentially no naming convention:
 * Fox.glb says b_LeftLeg01_015, CesiumMan.glb says leg_joint_L_1, and this engine's own generated
 * rig says thigh_L. Name matching would find nothing. Skeleton::assignRoles() maps all three onto
 * the same 17 humanoid roles using joint names AND hierarchy order, and retargeting then works
 * role-to-role. Whatever the role detector could not place is left unmapped and holds its rest pose,
 * and is listed in RetargetMap::missingRoles -- so a partial retarget says it is partial.
 *
 * REST-POSE ALIGNMENT
 * -------------------
 * Pasting a source rotation onto a destination joint is wrong whenever the two rest poses differ,
 * which they always do: Fox's legs are built for a quadruped, CesiumMan is a biped, and the engine's
 * rig binds in its own T-pose. So each joint's motion is transferred as its deviation from ITS OWN
 * rest rotation, re-applied to the destination's rest rotation:
 *
 *     out.r = dstRestR * (srcRestR^-1 * srcAnimR)
 *
 * A source at rest therefore maps to a destination at rest, whatever the two bind poses look like.
 * Where the destination expresses one role with several joints (a three-joint spine against a
 * one-joint spine), the deviation is split by RetargetMap::shareFactor so the total bend is
 * preserved rather than tripled; and where the SOURCE expresses it with several joints, their
 * individual deviations are composed, so the source spine's total bend is what transfers.
 *
 * Translation transfers for the Hips role only, scaled by the hip-height ratio. Every other joint
 * keeps the destination's own rest translation, because a source bone length means nothing on a
 * differently proportioned skeleton. Scale channels are not transferred at all.
 */
#include "af_anim.h"

#include <algorithm>
#include <cmath>

namespace af {
namespace anim {

namespace {

/** Roles must be assigned before a map can be built. Both inputs may be const in the caller's hands,
 *  so this works on copies only when needed and reports what it did. */
void ensureRoles(const Skeleton& sk, Skeleton* scratch, const Skeleton** used, std::string* note) {
    if (!sk.roleToJoint.empty() && sk.roleToJoint.size() == static_cast<size_t>(Role::COUNT)) {
        *used = &sk;
        return;
    }
    *scratch = sk;
    scratch->assignRoles();
    *used = scratch;
    if (note) *note = "roles were not assigned on this skeleton; they were derived for this mapping";
}

} /* namespace */

int RetargetMap::mappedJoints() const {
    int n = 0;
    for (int j : dstToSrc) if (j >= 0) ++n;
    return n;
}

bool buildRetargetMap(const Skeleton& srcIn, const Skeleton& dstIn, RetargetMap* out) {
    if (!out) return false;
    *out = RetargetMap();
    Skeleton srcScratch, dstScratch;
    const Skeleton* src = nullptr;
    const Skeleton* dst = nullptr;
    ensureRoles(srcIn, &srcScratch, &src, nullptr);
    ensureRoles(dstIn, &dstScratch, &dst, nullptr);
    if (!src || !dst || src->count() == 0 || dst->count() == 0) {
        out->notes.push_back("one of the two skeletons is empty");
        return false;
    }

    const int roleCount = static_cast<int>(Role::COUNT);
    out->srcRoleToJoint.assign(static_cast<size_t>(roleCount), -1);
    out->dstRoleToJoint.assign(static_cast<size_t>(roleCount), -1);
    for (int r = 0; r < roleCount; ++r) {
        out->srcRoleToJoint[static_cast<size_t>(r)] = src->jointForRole(static_cast<Role>(r));
        out->dstRoleToJoint[static_cast<size_t>(r)] = dst->jointForRole(static_cast<Role>(r));
    }

    int shared = 0;
    for (int r = 0; r < roleCount; ++r) {
        const bool haveSrc = out->srcRoleToJoint[static_cast<size_t>(r)] >= 0;
        const bool haveDst = out->dstRoleToJoint[static_cast<size_t>(r)] >= 0;
        if (haveSrc && haveDst) ++shared;
        else if (!haveSrc || !haveDst) {
            out->missingRoles.push_back(std::string(roleName(static_cast<Role>(r))) +
                                        (!haveSrc && !haveDst ? " (absent from both skeletons)"
                                         : (!haveSrc ? " (absent from the source)"
                                                     : " (absent from the destination)")));
        }
    }
    out->coverage = static_cast<float>(shared) / static_cast<float>(roleCount);

    /* Compatibility is a threshold on coverage AND the presence of a common root. Without Hips on
       both sides there is nothing to align the two rest poses against, and the result would be a
       pose floating at the origin however good the limb coverage looked. */
    const bool hipsBoth = out->srcRoleToJoint[0] >= 0 && out->dstRoleToJoint[0] >= 0;
    out->compatible = hipsBoth && out->coverage >= 0.5f;

    /* ---- per-destination-joint source joint and share factor ------------------------------ */
    out->dstToSrc.assign(static_cast<size_t>(dst->count()), -1);
    out->shareFactor.assign(static_cast<size_t>(dst->count()), 0.0f);
    for (int r = 0; r < roleCount; ++r) {
        const int srcJoint = out->srcRoleToJoint[static_cast<size_t>(r)];
        if (srcJoint < 0) continue;
        const std::vector<int> dstChain = dst->jointsForRole(static_cast<Role>(r));
        if (dstChain.empty()) continue;
        const float share = 1.0f / static_cast<float>(dstChain.size());
        for (int dj : dstChain) {
            if (dj < 0 || dj >= dst->count()) continue;
            out->dstToSrc[static_cast<size_t>(dj)] = srcJoint;
            out->shareFactor[static_cast<size_t>(dj)] = share;
        }
    }

    /* ---- rest-pose scale from hip height --------------------------------------------------- */
    const float srcHip = src->hipHeight();
    const float dstHip = dst->hipHeight();
    out->scale = (std::fabs(srcHip) > 1e-4f && std::fabs(dstHip) > 1e-4f) ? (dstHip / srcHip) : 1.0f;
    if (!std::isfinite(out->scale) || out->scale <= 0.0f) out->scale = 1.0f;

    char buf[192];
    std::snprintf(buf, sizeof(buf),
                  "%d of %d humanoid roles match on both skeletons (coverage %.0f%%); %d of %d "
                  "destination joints are driven",
                  shared, roleCount, out->coverage * 100.0f, out->mappedJoints(), dst->count());
    out->notes.push_back(buf);
    std::snprintf(buf, sizeof(buf), "hip height %.4f m -> %.4f m, translation scale %.4f",
                  srcHip, dstHip, out->scale);
    out->notes.push_back(buf);
    if (!hipsBoth) {
        out->notes.push_back("no common Hips joint: rest poses cannot be aligned, retargeting refused");
    }
    if (dst->count() > out->mappedJoints()) {
        std::snprintf(buf, sizeof(buf),
                      "%d destination joints hold their rest pose because no source joint maps to them",
                      dst->count() - out->mappedJoints());
        out->notes.push_back(buf);
    }
    return out->compatible;
}

void retargetPose(const Skeleton& srcIn, const Skeleton& dstIn, const RetargetMap& map,
                  const Pose& in, Pose* out) {
    if (!out) return;
    Skeleton srcScratch, dstScratch;
    const Skeleton* src = nullptr;
    const Skeleton* dst = nullptr;
    ensureRoles(srcIn, &srcScratch, &src, nullptr);
    ensureRoles(dstIn, &dstScratch, &dst, nullptr);
    if (!src || !dst) return;
    if (out->jointCount() != dst->count()) out->allocate(*dst, out->morphCount());
    out->resetToRest(*dst);
    if (!map.compatible) return;              /* an incompatible map yields the rest pose, not garbage */
    if (in.jointCount() != src->count()) return;

    const int roleCount = static_cast<int>(Role::COUNT);
    if (map.dstToSrc.size() != static_cast<size_t>(dst->count())) return;

    /* Compose the source deviation once per role: for a single-joint role this is exactly
       srcRestR^-1 * srcAnimR; for a multi-joint role it accumulates each bone's own deviation so
       the chain's total bend is what transfers. */
    thread_local std::vector<Quat> roleDelta;
    thread_local std::vector<char> roleValid;
    roleDelta.assign(static_cast<size_t>(roleCount), Quat::identity());
    roleValid.assign(static_cast<size_t>(roleCount), 0);
    for (int r = 0; r < roleCount; ++r) {
        const std::vector<int> srcChain = src->jointsForRole(static_cast<Role>(r));
        if (srcChain.empty()) continue;
        if (map.dstRoleToJoint.size() != static_cast<size_t>(roleCount)) continue;
        if (map.dstRoleToJoint[static_cast<size_t>(r)] < 0) continue;
        Quat delta = Quat::identity();
        for (int sj : srcChain) {
            if (sj < 0 || sj >= src->count()) continue;
            const Quat local = src->restR[static_cast<size_t>(sj)].conjugate() *
                               in.r[static_cast<size_t>(sj)];
            delta = delta * local.normalized();
        }
        roleDelta[static_cast<size_t>(r)] = delta.normalized();
        roleValid[static_cast<size_t>(r)] = 1;
    }

    for (int dj = 0; dj < dst->count(); ++dj) {
        const size_t j = static_cast<size_t>(dj);
        const int srcJoint = map.dstToSrc[j];
        if (srcJoint < 0) continue;                   /* holds its rest pose */
        const Role r = dst->roles[j];
        const int ri = static_cast<int>(r);
        if (ri < 0 || ri >= roleCount || !roleValid[static_cast<size_t>(ri)]) continue;
        const float share = (dj < static_cast<int>(map.shareFactor.size()))
                                ? map.shareFactor[static_cast<size_t>(dj)] : 1.0f;
        const Quat applied = (share >= 0.999f)
                                 ? roleDelta[static_cast<size_t>(ri)]
                                 : Quat::slerp(Quat::identity(), roleDelta[static_cast<size_t>(ri)], share);
        out->r[j] = (dst->restR[j] * applied).normalized();
    }

    /* Hips translation, scaled by the hip-height ratio. */
    const int dstHips = (map.dstRoleToJoint.size() == static_cast<size_t>(roleCount))
                            ? map.dstRoleToJoint[0] : -1;
    const int srcHips = (map.srcRoleToJoint.size() == static_cast<size_t>(roleCount))
                            ? map.srcRoleToJoint[0] : -1;
    if (dstHips >= 0 && srcHips >= 0) {
        const Vec3 srcDelta = in.t[static_cast<size_t>(srcHips)] - src->restT[static_cast<size_t>(srcHips)];
        out->t[static_cast<size_t>(dstHips)] = dst->restT[static_cast<size_t>(dstHips)] + srcDelta * map.scale;
    }

    /* Morph targets belong to the mesh, not the skeleton: a retargeted character keeps its own face.
       Copying the source's weights onto a mesh with different targets would be wrong, so they are
       deliberately left at the destination's rest (zero) and the caller drives the face separately. */
    out->matricesValid = false;
}

} /* namespace anim */
} /* namespace af */
