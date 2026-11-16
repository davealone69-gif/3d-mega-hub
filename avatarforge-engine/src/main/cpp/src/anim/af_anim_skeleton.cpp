/*
 * af_anim_skeleton.cpp -- skeleton construction, matrix decomposition and humanoid role detection.
 *
 * The role detector is the part of retargeting that decides whether the feature is real or
 * decorative. It runs against joint names that no two exporters agree on -- the three skeletons the
 * host test suite uses are:
 *
 *   Fox.glb (Khronos, CC0/CC-BY-4.0)     _rootJoint, b_Root_00, b_Hip_01, b_Spine01_02, b_Spine02_03,
 *                                        b_Neck_04, b_Head_05, b_LeftUpperArm_09, b_LeftForeArm_010,
 *                                        b_LeftHand_011, b_LeftLeg01_015, b_LeftLeg02_016,
 *                                        b_LeftFoot01_017, b_Tail01_012 ...
 *   CesiumMan.glb (Khronos, CC-BY-4.0)   Skeleton_torso_joint_1..3, Skeleton_neck_joint_1..2,
 *                                        Skeleton_arm_joint_L__4_/_L__3_/_L__2_, leg_joint_L_1..5
 *   af::Rig (this engine, 75 joints)     root, hips, spine_01..03, chest, neck_01, head, jaw,
 *                                        eye_L/R, clavicle_L, upperarm_01_L, upperarm_02_L,
 *                                        forearm_L, hand_L, thigh_L, thigh_twist_L, shin_L, foot_L
 *
 * Note what those three have in common: almost nothing. CesiumMan has no head joint at all and its
 * limb ordering lives in the hierarchy rather than the names (arm_joint_L__4_ is the SHOULDER, not
 * the fourth arm bone). Fox numbers its spine from 01 and its legs from 01. So names alone cannot
 * work, and hierarchy alone cannot work either -- the detector uses both, in this order:
 *
 *   1. tokenise the name (camelCase and separators, digits dropped, side detected from l/r/left/right);
 *   2. classify each joint into a chain family (spine, neck/head, arm, leg, ignore);
 *   3. within a family and a side, let an unambiguous keyword claim its role (thigh -> UpperLeg,
 *      forearm -> LowerArm, foot -> Foot) and then fill the roles nobody claimed by walking the
 *      hierarchy from the body outward, which is what makes CesiumMan's arm_joint_L__4_/_3_/_2_
 *      come out as shoulder/elbow/wrist despite the misleading numbers;
 *   4. the spine chain shares roles across its bend joints (Fox's _rootJoint/b_Root_00/b_Hip_01 are
 *      all Hips), because that is one physical chain and splitting the motion across it is what a
 *      retarget has to do. Limbs do NOT share: a second "upperarm" or "foot" bone is a bend or toe
 *      correction and is left unmapped rather than double-applying a rotation.
 *
 * Anything the detector cannot place stays Role::COUNT and is reported through
 * RetargetMap::missingRoles. It is never guessed from the character's name or from the file's
 * generator string.
 */
#include "af_anim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace af {
namespace anim {

/* ================================================================================== names ====== */

const char* interpName(Interp i) {
    switch (i) {
        case Interp::Linear: return "LINEAR";
        case Interp::Step: return "STEP";
        case Interp::CubicSpline: return "CUBICSPLINE";
        case Interp::Smoothstep: return "SMOOTHSTEP";
    }
    return "UNKNOWN";
}

const char* pathName(Path p) {
    switch (p) {
        case Path::Translation: return "translation";
        case Path::Rotation: return "rotation";
        case Path::Scale: return "scale";
        case Path::Weights: return "weights";
    }
    return "unknown";
}

const char* roleName(Role r) {
    switch (r) {
        case Role::Hips: return "Hips";
        case Role::Spine: return "Spine";
        case Role::Chest: return "Chest";
        case Role::Neck: return "Neck";
        case Role::Head: return "Head";
        case Role::LeftUpperLeg: return "LeftUpperLeg";
        case Role::LeftLowerLeg: return "LeftLowerLeg";
        case Role::LeftFoot: return "LeftFoot";
        case Role::RightUpperLeg: return "RightUpperLeg";
        case Role::RightLowerLeg: return "RightLowerLeg";
        case Role::RightFoot: return "RightFoot";
        case Role::LeftUpperArm: return "LeftUpperArm";
        case Role::LeftLowerArm: return "LeftLowerArm";
        case Role::LeftHand: return "LeftHand";
        case Role::RightUpperArm: return "RightUpperArm";
        case Role::RightLowerArm: return "RightLowerArm";
        case Role::RightHand: return "RightHand";
        case Role::COUNT: return "<unmapped>";
    }
    return "<invalid role>";
}

bool roleIsLeftSide(Role r) {
    return r == Role::LeftUpperLeg || r == Role::LeftLowerLeg || r == Role::LeftFoot ||
           r == Role::LeftUpperArm || r == Role::LeftLowerArm || r == Role::LeftHand;
}

bool roleIsRightSide(Role r) {
    return r == Role::RightUpperLeg || r == Role::RightLowerLeg || r == Role::RightFoot ||
           r == Role::RightUpperArm || r == Role::RightLowerArm || r == Role::RightHand;
}

Role roleMirrored(Role r) {
    switch (r) {
        case Role::LeftUpperLeg: return Role::RightUpperLeg;
        case Role::LeftLowerLeg: return Role::RightLowerLeg;
        case Role::LeftFoot: return Role::RightFoot;
        case Role::LeftUpperArm: return Role::RightUpperArm;
        case Role::LeftLowerArm: return Role::RightLowerArm;
        case Role::LeftHand: return Role::RightHand;
        case Role::RightUpperLeg: return Role::LeftUpperLeg;
        case Role::RightLowerLeg: return Role::LeftLowerLeg;
        case Role::RightFoot: return Role::LeftFoot;
        case Role::RightUpperArm: return Role::LeftUpperArm;
        case Role::RightLowerArm: return Role::LeftLowerArm;
        case Role::RightHand: return Role::LeftHand;
        default: return r;
    }
}

/* ==================================================================== matrix decomposition ==== */

Quat quatFromRotationMatrix(const Vec3& col0, const Vec3& col1, const Vec3& col2) {
    /* Columns are the basis vectors, so element (row, col) is colN[row]:
         R00 = col0.x  R01 = col1.x  R02 = col2.x
         R10 = col0.y  R11 = col1.y  R12 = col2.y
         R20 = col0.z  R21 = col1.z  R22 = col2.z                                            */
    const float r00 = col0.x, r01 = col1.x, r02 = col2.x;
    const float r10 = col0.y, r11 = col1.y, r12 = col2.y;
    const float r20 = col0.z, r21 = col1.z, r22 = col2.z;
    const float trace = r00 + r11 + r22;
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        if (s < kEps) return Quat::identity();
        q.w = 0.25f * s;
        q.x = (r21 - r12) / s;
        q.y = (r02 - r20) / s;
        q.z = (r10 - r01) / s;
    } else if (r00 > r11 && r00 > r22) {
        const float s = std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
        if (s < kEps) return Quat::identity();
        q.w = (r21 - r12) / s;
        q.x = 0.25f * s;
        q.y = (r01 + r10) / s;
        q.z = (r02 + r20) / s;
    } else if (r11 > r22) {
        const float s = std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
        if (s < kEps) return Quat::identity();
        q.w = (r02 - r20) / s;
        q.x = (r01 + r10) / s;
        q.y = 0.25f * s;
        q.z = (r12 + r21) / s;
    } else {
        const float s = std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
        if (s < kEps) return Quat::identity();
        q.w = (r10 - r01) / s;
        q.x = (r02 + r20) / s;
        q.y = (r12 + r21) / s;
        q.z = 0.25f * s;
    }
    return q.normalized();
}

bool decomposeTrs(const Mat4& m, Vec3* outT, Quat* outR, Vec3* outS) {
    if (!outT || !outR || !outS) return false;
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(m.m[i])) return false;
    }
    const Vec3 col0(m.m[0], m.m[1], m.m[2]);
    const Vec3 col1(m.m[4], m.m[5], m.m[6]);
    const Vec3 col2(m.m[8], m.m[9], m.m[10]);
    const float len0 = col0.length();
    const float len1 = col1.length();
    const float len2 = col2.length();
    if (len0 < kEps || len1 < kEps || len2 < kEps) return false;   /* a zero basis column has no rotation */
    float sx = len0, sy = len1, sz = len2;
    /* A mirrored bone (negative determinant) carries the sign in one axis. Put it on X, the
       convention Mat4::trs round-trips, so the remaining basis is a proper rotation. */
    if (col0.dot(col1.cross(col2)) < 0.0f) sx = -sx;
    const Vec3 b0 = col0 * (1.0f / sx);
    const Vec3 b1 = col1 * (1.0f / sy);
    const Vec3 b2 = col2 * (1.0f / sz);
    const Quat q = quatFromRotationMatrix(b0, b1, b2);
    if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w)) return false;
    *outT = Vec3(m.m[12], m.m[13], m.m[14]);
    *outR = q;
    *outS = Vec3(sx, sy, sz);
    return true;
}

/* ============================================================================ tokenisation ==== */

namespace {

enum class Side { Midline, Left, Right };

struct JointTokens {
    std::vector<std::string> tokens;
    Side side = Side::Midline;
    bool has(const char* t) const {
        for (const auto& s : tokens) if (s == t) return true;
        return false;
    }
    /** True when `a` is immediately followed by `b`, so "upper arm" is distinguished from a joint
     *  that merely mentions both words somewhere. */
    bool bigram(const char* a, const char* b) const {
        for (size_t i = 0; i + 1 < tokens.size(); ++i) {
            if (tokens[i] == a && tokens[i + 1] == b) return true;
        }
        return false;
    }
    bool any(std::initializer_list<const char*> list) const {
        for (const char* t : list) if (has(t)) return true;
        return false;
    }
};

/** Splits a joint name into lowercase alphabetic tokens: camelCase boundaries and separators both
 *  split, digits are dropped. "b_LeftUpperArm_09" -> {b, left, upper, arm}. */
JointTokens tokenise(const std::string& name) {
    JointTokens out;
    std::string cur;
    auto flush = [&]() {
        if (cur.empty()) return;
        std::string lower;
        lower.reserve(cur.size());
        for (char c : cur) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        /* Split an already-lowercased run on original-case boundaries is not possible any more, so
           the camelCase split happens on `cur` before lowering. */
        out.tokens.push_back(lower);
        cur.clear();
    };
    for (size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool upper = (c >= 'A' && c <= 'Z');
        const bool lower = (c >= 'a' && c <= 'z');
        const bool digit = (c >= '0' && c <= '9');
        if (!upper && !lower) { flush(); continue; }        /* separator */
        if (digit) { flush(); continue; }                   /* numbering carries no role meaning */
        if (upper && !cur.empty()) {
            const char prev = name[i - 1];
            const bool prevLower = (prev >= 'a' && prev <= 'z');
            const bool nextLower = (i + 1 < name.size()) && (name[i + 1] >= 'a' && name[i + 1] <= 'z');
            const bool prevUpper = (prev >= 'A' && prev <= 'Z');
            if (prevLower || (prevUpper && nextLower)) flush();   /* camelCase boundary */
        }
        cur.push_back(c);
    }
    flush();

    out.side = Side::Midline;
    for (const auto& t : out.tokens) {
        if (t == "left" || t == "l") out.side = Side::Left;
        else if (t == "right" || t == "r") out.side = Side::Right;
    }
    return out;
}

enum class Family { None, Spine, NeckHead, Arm, Leg };

/** Which chain a joint belongs to, from its tokens. A joint in no family cannot take a role. */
Family familyOf(const JointTokens& tk) {
    /* Explicitly excluded first: a tail, a jaw, an eye, an IK handle or a twist/bend correction
       bone is not part of the humanoid core, and pretending otherwise produces a retarget that
       looks plausible and moves the wrong thing. */
    if (tk.any({"tail", "jaw", "eye", "eyelid", "brow", "cheek", "mouth", "lip", "nose", "tongue",
                "teeth", "ik", "pole", "twist", "bend", "corrective", "end", "tip", "toe", "thumb",
                "index", "middle", "ring", "little", "pinky", "finger", "hair", "cloth", "garment"})) {
        return Family::None;
    }
    /* "hip" is genuinely ambiguous across rigs and has to be resolved on side, not guessed. A
       MIDLINE Hips/pelvis is the top of the spine chain and the parent of both legs -- putting it in
       the leg family left Fox's b_Hip_01 with no role at all, because the limb passes only look at
       left and right, and the retargeter then drove its neighbours while leaving the pelvis at rest:
       a visible kink in the animal's back. A SIDED hip_L / hip_R is how some rigs name the thigh, so
       that stays a leg. */
    if (tk.any({"hip", "hips", "pelvis"}) && tk.side == Side::Midline) return Family::Spine;
    if (tk.any({"head", "skull", "cranium"})) return Family::NeckHead;
    if (tk.any({"neck", "cervical", "nape"})) return Family::NeckHead;
    if (tk.any({"arm", "shoulder", "forearm", "elbow", "hand", "wrist", "clavicle", "collar"})) return Family::Arm;
    if (tk.any({"leg", "thigh", "shin", "calf", "knee", "foot", "ankle", "hip", "pelvis"})) return Family::Leg;
    if (tk.any({"spine", "chest", "thorax", "torso", "root", "waist", "abdomen", "back", "stomach"})) return Family::Spine;
    return Family::None;
}

/** Depth of a joint in the closure, i.e. how many ancestors it has. -1 for an out-of-range index. */
int depthOf(const Skeleton& sk, int joint) {
    int depth = 0;
    int cur = joint;
    int guard = 0;
    while (cur >= 0 && cur < sk.count()) {
        const int p = sk.parents[static_cast<size_t>(cur)];
        if (p < 0) break;
        cur = p;
        if (++depth > sk.count() || ++guard > sk.count() + 1) return -1;   /* cycle: report, do not loop */
    }
    return depth;
}

void sortByDepth(const Skeleton& sk, std::vector<int>* joints) {
    std::stable_sort(joints->begin(), joints->end(), [&](int a, int b) {
        return depthOf(sk, a) < depthOf(sk, b);
    });
}

/** True when `candidate` is a descendant of `ancestor`. */
bool isDescendant(const Skeleton& sk, int candidate, int ancestor) {
    int cur = candidate;
    int guard = 0;
    while (cur >= 0 && cur < sk.count()) {
        if (cur == ancestor) return true;
        cur = sk.parents[static_cast<size_t>(cur)];
        if (++guard > sk.count() + 1) return false;
    }
    return false;
}

} /* namespace */

/* ================================================================================ Skeleton ==== */

int Skeleton::find(const std::string& name) const {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) return static_cast<int>(i);
    }
    return -1;
}

namespace {
/** Lowercases and strips separators so "UpperArm_01_L" and "upperarm01l" compare equal. */
std::string normaliseName(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out.push_back(c);
        else if (c >= 'A' && c <= 'Z') out.push_back(static_cast<char>(c - 'A' + 'a'));
    }
    return out;
}
} /* namespace */

int Skeleton::findFuzzy(const std::string& name) const {
    const std::string target = normaliseName(name);
    if (target.empty()) return -1;
    for (size_t i = 0; i < names.size(); ++i) {
        if (normaliseName(names[i]) == target) return static_cast<int>(i);
    }
    return -1;
}

int Skeleton::rolesMatched() const {
    if (roleToJoint.size() != static_cast<size_t>(Role::COUNT)) return 0;
    int found = 0;
    for (int r = 0; r < static_cast<int>(Role::COUNT); ++r) {
        if (roleToJoint[static_cast<size_t>(r)] >= 0) ++found;
    }
    return found;
}

int Skeleton::jointForRole(Role r) const {
    const int ri = static_cast<int>(r);
    if (ri < 0 || ri >= static_cast<int>(Role::COUNT)) return -1;
    if (roleToJoint.size() != static_cast<size_t>(Role::COUNT)) return -1;
    return roleToJoint[static_cast<size_t>(ri)];
}

std::vector<int> Skeleton::jointsForRole(Role r) const {
    std::vector<int> out;
    const int ri = static_cast<int>(r);
    if (ri < 0 || ri >= static_cast<int>(Role::COUNT)) return out;
    if (roleChainOffset.size() != static_cast<size_t>(Role::COUNT) + 1) return out;
    const int begin = roleChainOffset[static_cast<size_t>(ri)];
    const int end = roleChainOffset[static_cast<size_t>(ri) + 1];
    if (begin < 0 || end < begin || end > static_cast<int>(roleChain.size())) return out;
    out.assign(roleChain.begin() + begin, roleChain.begin() + end);
    return out;
}

bool Skeleton::validate(std::string* error) const {
    const size_t n = names.size();
    auto fail = [&](const char* msg) { if (error) *error = msg; return false; };
    if (n == 0) return fail("skeleton has no joints");
    if (parents.size() != n || restT.size() != n || restR.size() != n || restS.size() != n) {
        return fail("skeleton arrays are not the same length");
    }
    if (order.size() != n) return fail("evaluation order does not cover every joint exactly once");
    std::vector<char> seen(n, 0);
    for (int i : order) {
        if (i < 0 || i >= static_cast<int>(n)) return fail("evaluation order contains an out-of-range joint");
        if (seen[static_cast<size_t>(i)]) return fail("evaluation order repeats a joint");
        seen[static_cast<size_t>(i)] = 1;
        const int p = parents[static_cast<size_t>(i)];
        if (p >= 0) {
            if (p >= static_cast<int>(n)) return fail("joint parent index is out of range");
            if (p == i) return fail("joint is its own parent");
            if (!seen[static_cast<size_t>(p)]) return fail("evaluation order places a child before its parent");
        }
        if (!restT[static_cast<size_t>(i)].isFinite()) return fail("joint rest translation is not finite");
        const Quat& q = restR[static_cast<size_t>(i)];
        if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w)) {
            return fail("joint rest rotation is not finite");
        }
        if (std::fabs(q.dot(q)) < 1e-8f) return fail("joint rest rotation is a zero quaternion");
    }
    for (size_t i = 0; i < n; ++i) if (!seen[i]) return fail("evaluation order omits a joint");
    if (!inverseBind.empty() && inverseBind.size() != n) {
        return fail("inverse bind matrices are not parallel to the joints");
    }
    if (!jointToSkinSlot.empty() && jointToSkinSlot.size() != n) {
        return fail("joint-to-skin-slot map is not parallel to the joints");
    }
    if (!skinSlotToJoint.empty()) {
        for (int j : skinSlotToJoint) {
            if (j < 0 || j >= static_cast<int>(n)) return fail("skin slot addresses a joint outside the skeleton");
        }
    }
    return true;
}

Vec3 Skeleton::restWorldPosition(int joint) const {
    if (joint < 0 || joint >= count()) return Vec3(0, 0, 0);
    Mat4 world = Mat4::trs(restT[static_cast<size_t>(joint)], restR[static_cast<size_t>(joint)],
                           restS[static_cast<size_t>(joint)]);
    int cur = parents[static_cast<size_t>(joint)];
    int guard = 0;
    while (cur >= 0 && cur < count() && guard++ <= count()) {
        const Mat4 local = Mat4::trs(restT[static_cast<size_t>(cur)], restR[static_cast<size_t>(cur)],
                                     restS[static_cast<size_t>(cur)]);
        world = local * world;
        cur = parents[static_cast<size_t>(cur)];
    }
    return world.transformPoint(Vec3(0, 0, 0));
}

float Skeleton::boneLength(int joint) const {
    if (joint < 0 || joint >= count()) return 0.0f;
    float best = 0.0f;
    bool found = false;
    const Vec3 origin = restWorldPosition(joint);
    for (int i = 0; i < count(); ++i) {
        if (parents[static_cast<size_t>(i)] != joint) continue;
        const float d = origin.distanceTo(restWorldPosition(i));
        if (!found || d < best) { best = d; found = true; }
    }
    return found ? best : 0.0f;
}

float Skeleton::hipHeight() const {
    const int hips = jointForRole(Role::Hips);
    if (hips < 0) return 0.0f;
    const float h = restWorldPosition(hips).y;
    return std::fabs(h) < kEps ? 0.0f : h;
}

void Skeleton::buildPalette(const Pose& pose, std::vector<Mat4>* palette) const {
    if (!palette) return;
    palette->clear();
    if (!hasInverseBind || skinSlotToJoint.empty()) return;
    palette->reserve(skinSlotToJoint.size());
    for (int slotJoint : skinSlotToJoint) {
        if (slotJoint < 0 || slotJoint >= static_cast<int>(pose.world.size())) {
            palette->push_back(Mat4::identity());
            continue;
        }
        palette->push_back(meshNodeInverse * pose.world[static_cast<size_t>(slotJoint)] *
                           inverseBind[static_cast<size_t>(slotJoint)]);
    }
}

int Skeleton::assignRoles() {
    const int n = count();
    roles.assign(static_cast<size_t>(n), Role::COUNT);
    roleToJoint.assign(static_cast<size_t>(Role::COUNT), -1);
    roleChain.clear();
    roleChainOffset.assign(static_cast<size_t>(Role::COUNT) + 1, 0);
    if (n == 0) return 0;

    std::vector<JointTokens> tokens(static_cast<size_t>(n));
    std::vector<Family> family(static_cast<size_t>(n), Family::None);
    std::vector<int> depth(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; ++i) {
        tokens[static_cast<size_t>(i)] = tokenise(names[static_cast<size_t>(i)]);
        family[static_cast<size_t>(i)] = familyOf(tokens[static_cast<size_t>(i)]);
        depth[static_cast<size_t>(i)] = depthOf(*this, i);
    }

    /* `claimed` mirrors roleToJoint during the passes below. roleToJoint itself is only filled at
       the end (from the finished roles array) so that the chain lists and the primary joint cannot
       disagree with each other. */
    std::vector<int> claimed(static_cast<size_t>(Role::COUNT), -1);
    auto assign = [&](int joint, Role r, bool allowShared) {
        if (joint < 0 || joint >= n) return false;
        const int ri = static_cast<int>(r);
        if (claimed[static_cast<size_t>(ri)] >= 0 && !allowShared) return false;
        if (roles[static_cast<size_t>(joint)] != Role::COUNT) return false;
        roles[static_cast<size_t>(joint)] = r;
        if (claimed[static_cast<size_t>(ri)] < 0) claimed[static_cast<size_t>(ri)] = joint;
        return true;
    };
    auto isClaimedJoint = [&](int joint) { return roles[static_cast<size_t>(joint)] != Role::COUNT; };

    /* ---- 1. spine chain ------------------------------------------------------------------ */
    std::vector<int> spineChain;
    for (int i = 0; i < n; ++i) {
        if (family[static_cast<size_t>(i)] == Family::Spine && tokens[static_cast<size_t>(i)].side == Side::Midline) {
            spineChain.push_back(i);
        }
    }
    sortByDepth(*this, &spineChain);
    int hipsPrimary = -1;
    if (!spineChain.empty()) {
        /* An explicit hip/pelvis token wins over "the shallowest torso joint", because that is the
           anatomical hips; every spine-family joint at or above it is part of the same physical
           chain and shares the Hips role. */
        int hipTokenIndex = -1;
        for (size_t k = 0; k < spineChain.size(); ++k) {
            if (tokens[static_cast<size_t>(spineChain[k])].any({"hip", "hips", "pelvis"})) {
                hipTokenIndex = static_cast<int>(k);
                break;
            }
        }
        const int hipDepth = (hipTokenIndex >= 0)
            ? depth[static_cast<size_t>(spineChain[static_cast<size_t>(hipTokenIndex)])]
            : depth[static_cast<size_t>(spineChain.front())];
        size_t splitAt = 0;
        for (size_t k = 0; k < spineChain.size(); ++k) {
            if (depth[static_cast<size_t>(spineChain[k])] <= hipDepth) splitAt = k + 1;
        }
        if (splitAt == 0) splitAt = 1;
        /* Spine-family roles are shared: Fox's _rootJoint / b_Root_00 / b_Hip_01 are one physical
           chain, and the retargeter splits a source role's rotation across whatever joints the
           destination gives it. */
        for (size_t k = 0; k < splitAt && k < spineChain.size(); ++k) assign(spineChain[k], Role::Hips, true);
        hipsPrimary = spineChain.front();
        if (spineChain.size() > splitAt) {
            for (size_t k = splitAt; k + 1 < spineChain.size(); ++k) assign(spineChain[k], Role::Spine, true);
            assign(spineChain.back(), Role::Chest, true);
        }
    }

    /* ---- 2. neck and head ---------------------------------------------------------------- */
    std::vector<int> neckChain;
    std::vector<int> headChain;
    for (int i = 0; i < n; ++i) {
        if (family[static_cast<size_t>(i)] != Family::NeckHead) continue;
        if (tokens[static_cast<size_t>(i)].any({"head", "skull", "cranium"})) headChain.push_back(i);
        else neckChain.push_back(i);
    }
    sortByDepth(*this, &neckChain);
    sortByDepth(*this, &headChain);
    for (int j : neckChain) assign(j, Role::Neck, true);
    if (!headChain.empty()) assign(headChain.front(), Role::Head, false);

    /* ---- 3. and 4. limbs ----------------------------------------------------------------- */
    struct LimbSpec {
        Family family;
        Side side;
        Role upper, lower, terminal;
        std::initializer_list<const char*> upperKeywords;
        std::initializer_list<const char*> lowerKeywords;
        std::initializer_list<const char*> terminalKeywords;
    };
    const LimbSpec limbs[] = {
        {Family::Leg, Side::Left, Role::LeftUpperLeg, Role::LeftLowerLeg, Role::LeftFoot,
         {"thigh", "upleg", "upperleg"}, {"shin", "calf", "lowerleg", "knee"}, {"foot", "ankle"}},
        {Family::Leg, Side::Right, Role::RightUpperLeg, Role::RightLowerLeg, Role::RightFoot,
         {"thigh", "upleg", "upperleg"}, {"shin", "calf", "lowerleg", "knee"}, {"foot", "ankle"}},
        {Family::Arm, Side::Left, Role::LeftUpperArm, Role::LeftLowerArm, Role::LeftHand,
         {"upperarm"}, {"forearm", "lowerarm", "elbow"}, {"hand", "wrist"}},
        {Family::Arm, Side::Right, Role::RightUpperArm, Role::RightLowerArm, Role::RightHand,
         {"upperarm"}, {"forearm", "lowerarm", "elbow"}, {"hand", "wrist"}},
    };

    for (const LimbSpec& spec : limbs) {
        /* A limb must hang off the body: arms off the Chest, legs off the Hips. Requiring that is
           what stops a stray prop node named "foot" from being adopted as the character's ankle,
           and it is why the spine chain is resolved before the limbs are. */
        const int anchor = (spec.family == Family::Arm) ? claimed[static_cast<size_t>(Role::Chest)]
                                                        : hipsPrimary;
        std::vector<int> members;
        for (int i = 0; i < n; ++i) {
            if (family[static_cast<size_t>(i)] != spec.family) continue;
            if (tokens[static_cast<size_t>(i)].side != spec.side) continue;
            if (anchor >= 0 && !isDescendant(*this, i, anchor)) continue;
            members.push_back(i);
        }
        sortByDepth(*this, &members);
        if (members.empty()) continue;

        /* Pass 1: unambiguous keywords claim their role, first in hierarchy order. Limbs do not
           share a role -- a second "upperarm" or "foot" bone is a bend or toe correction, and
           mapping it too would apply the same rotation twice. */
        for (int j : members) {
            const JointTokens& tk = tokens[static_cast<size_t>(j)];
            const bool upperBigram = (spec.family == Family::Arm) ? tk.bigram("upper", "arm")
                                                                  : tk.bigram("upper", "leg");
            const bool lowerBigram = (spec.family == Family::Arm)
                                         ? (tk.bigram("fore", "arm") || tk.bigram("lower", "arm"))
                                         : tk.bigram("lower", "leg");
            Role want = Role::COUNT;
            if (tk.any(spec.upperKeywords) || upperBigram) want = spec.upper;
            else if (tk.any(spec.lowerKeywords) || lowerBigram) want = spec.lower;
            else if (tk.any(spec.terminalKeywords)) want = spec.terminal;
            if (want == Role::COUNT) continue;
            assign(j, want, false);
        }
        /* Pass 2: fill whatever nobody claimed, walking outward from the body. This is the path
           CesiumMan takes: its arm joints are named _L__4_ / _L__3_ / _L__2_ and carry no
           upper/fore/hand keyword at all, so only their order in the hierarchy can say which is
           the shoulder, which the elbow and which the wrist. */
        const Role fillOrder[3] = {spec.upper, spec.lower, spec.terminal};
        for (Role want : fillOrder) {
            if (claimed[static_cast<size_t>(want)] >= 0) continue;
            for (int j : members) {
                if (isClaimedJoint(j)) continue;
                if (assign(j, want, false)) break;
            }
        }
    }

    /* ---- 6. unnamed link bones inside a labelled spine chain ------------------------------ */
    /* Exporters routinely insert an unlabelled bone between two named ones ("b_Spine01" -> "bone_017"
       -> "b_Spine02"). Left unmapped, such a joint holds its rest rotation while both neighbours are
       driven, which shows up as a kink. It is only safe to fill when the file itself labelled both
       sides with the SAME spine-family role: that is the file telling us the joint is part of that
       chain. Limbs, tails and fingers never qualify, so this cannot steal a role from one. */
    for (int i = 0; i < n; ++i) {
        if (roles[static_cast<size_t>(i)] != Role::COUNT) continue;
        const int parent = parents[static_cast<size_t>(i)];
        if (parent < 0 || parent >= n) continue;
        const Role pr = roles[static_cast<size_t>(parent)];
        if (pr != Role::Hips && pr != Role::Spine && pr != Role::Chest && pr != Role::Neck) continue;
        bool childAgrees = false;
        for (int c = 0; c < n; ++c) {
            if (parents[static_cast<size_t>(c)] != i) continue;
            if (roles[static_cast<size_t>(c)] == pr) { childAgrees = true; break; }
        }
        if (childAgrees) roles[static_cast<size_t>(i)] = pr;
    }

    /* ---- roleToJoint / roleChain, both rebuilt from the finished roles array --------------- */
    for (int r = 0; r < static_cast<int>(Role::COUNT); ++r) {
        roleChainOffset[static_cast<size_t>(r)] = static_cast<int>(roleChain.size());
        std::vector<int> chain;
        for (int i = 0; i < n; ++i) {
            if (static_cast<int>(roles[static_cast<size_t>(i)]) == r) chain.push_back(i);
        }
        sortByDepth(*this, &chain);
        for (int j : chain) roleChain.push_back(j);
        if (chain.empty()) {
            roleToJoint[static_cast<size_t>(r)] = -1;
        } else if (static_cast<Role>(r) == Role::Hips) {
            /* The Hips chain runs from the scene container down to the pelvis, and the PELVIS is the
               primary: it is where the legs attach and it is what hipHeight() has to measure to give
               retargeting a real scale factor. Taking the shallowest joint instead measured Fox's
               `root` node at the origin and reported a hip height of zero. */
            roleToJoint[static_cast<size_t>(r)] = chain.back();
        } else {
            roleToJoint[static_cast<size_t>(r)] = chain.front();
        }
    }
    roleChainOffset[static_cast<size_t>(Role::COUNT)] = static_cast<int>(roleChain.size());

    int found = 0;
    for (int r = 0; r < static_cast<int>(Role::COUNT); ++r) {
        if (roleToJoint[static_cast<size_t>(r)] >= 0) ++found;
    }
    return found;
}

/* --------------------------------------------------------------------------- fromGlb / fromRig */

bool Skeleton::fromGlb(const GlbAsset& asset, int skinIndex, Skeleton* out, std::string* error) {
    auto fail = [&](const char* msg) { if (error) *error = msg; return false; };
    if (!out) return fail("Skeleton::fromGlb: null output");
    *out = Skeleton();
    if (skinIndex < 0 || skinIndex >= static_cast<int>(asset.skins.size())) {
        return fail("skin index is outside the imported document's skins");
    }
    const ImportedSkin& skin = asset.skins[static_cast<size_t>(skinIndex)];
    const int nodeCount = static_cast<int>(asset.nodes.size());
    if (skin.joints.empty()) return fail("the imported skin declares no joints");
    for (int j : skin.joints) {
        if (j < 0 || j >= nodeCount) return fail("the imported skin references a node outside the document");
    }

    /* ---- joint closure: every joint plus every ancestor node ----------------------------- */
    std::vector<char> inSet(static_cast<size_t>(nodeCount), 0);
    for (int j : skin.joints) {
        int cur = j;
        int guard = 0;
        while (cur >= 0 && cur < nodeCount && !inSet[static_cast<size_t>(cur)]) {
            inSet[static_cast<size_t>(cur)] = 1;
            cur = asset.nodes[static_cast<size_t>(cur)].parent;
            if (++guard > nodeCount + 1) break;      /* defensive: importGlb already rejects cycles */
        }
    }
    std::vector<int> closure;
    std::vector<int> skelOfNode(static_cast<size_t>(nodeCount), -1);
    for (int ni = 0; ni < nodeCount; ++ni) {
        if (!inSet[static_cast<size_t>(ni)]) continue;
        skelOfNode[static_cast<size_t>(ni)] = static_cast<int>(closure.size());
        closure.push_back(ni);
    }
    const int m = static_cast<int>(closure.size());
    if (m == 0) return fail("the imported skin produced an empty joint closure");

    out->names.reserve(static_cast<size_t>(m));
    out->parents.assign(static_cast<size_t>(m), -1);
    out->jointToSkinSlot.assign(static_cast<size_t>(m), -1);
    out->inverseBind.assign(static_cast<size_t>(m), Mat4::identity());
    out->sourceNode = closure;
    for (size_t i = 0; i < closure.size(); ++i) {
        const ImportedNode& node = asset.nodes[static_cast<size_t>(closure[i])];
        out->names.push_back(node.name.empty() ? ("<node " + std::to_string(closure[i]) + ">") : node.name);
        const int p = node.parent;
        if (p >= 0 && p < nodeCount && skelOfNode[static_cast<size_t>(p)] >= 0) {
            out->parents[i] = skelOfNode[static_cast<size_t>(p)];
        }
    }
    for (size_t slot = 0; slot < skin.joints.size(); ++slot) {
        const int skel = skelOfNode[static_cast<size_t>(skin.joints[slot])];
        if (skel < 0) return fail("a skin joint did not enter the closure");
        out->jointToSkinSlot[static_cast<size_t>(skel)] = static_cast<int>(slot);
        out->skinSlotToJoint.push_back(skel);
    }
    if (!skin.inverseBindMatrices.empty()) {
        if (skin.inverseBindMatrices.size() != skin.joints.size()) {
            return fail("inverseBindMatrices count does not match the skin's joint count");
        }
        for (size_t slot = 0; slot < skin.joints.size(); ++slot) {
            const int skel = out->skinSlotToJoint[slot];
            out->inverseBind[static_cast<size_t>(skel)] = skin.inverseBindMatrices[slot];
        }
        out->hasInverseBind = true;
    }

    /* ---- parents-before-children evaluation order ---------------------------------------- */
    out->order.clear();
    out->order.reserve(static_cast<size_t>(m));
    std::vector<char> placed(static_cast<size_t>(m), 0);
    bool progress = true;
    while (out->order.size() < static_cast<size_t>(m) && progress) {
        progress = false;
        for (int i = 0; i < m; ++i) {
            if (placed[static_cast<size_t>(i)]) continue;
            const int p = out->parents[static_cast<size_t>(i)];
            if (p >= 0 && !placed[static_cast<size_t>(p)]) continue;
            placed[static_cast<size_t>(i)] = 1;
            out->order.push_back(i);
            progress = true;
        }
    }
    if (out->order.size() != static_cast<size_t>(m)) {
        return fail("the imported joint hierarchy contains a cycle");
    }

    /* ---- rest local TRS ------------------------------------------------------------------- */
    out->restT.assign(static_cast<size_t>(m), Vec3(0, 0, 0));
    out->restR.assign(static_cast<size_t>(m), Quat::identity());
    out->restS.assign(static_cast<size_t>(m), Vec3(1, 1, 1));
    for (int i = 0; i < m; ++i) {
        Vec3 t; Quat r; Vec3 s;
        if (decomposeTrs(asset.nodes[static_cast<size_t>(closure[static_cast<size_t>(i)])].local, &t, &r, &s)) {
            out->restT[static_cast<size_t>(i)] = t;
            out->restR[static_cast<size_t>(i)] = r;
            out->restS[static_cast<size_t>(i)] = s;
        } else {
            /* An undecomposable bind matrix is a real defect in the file. Say so instead of
               silently binding the joint to the origin, which would collapse the limb. */
            return fail("a joint's bind transform could not be decomposed into TRS");
        }
    }

    /* ---- the mesh node this skin deforms, for the skinning space -------------------------- */
    for (int ni = 0; ni < nodeCount; ++ni) {
        if (asset.nodes[static_cast<size_t>(ni)].skin != skinIndex) continue;
        if (asset.nodes[static_cast<size_t>(ni)].mesh < 0) continue;
        out->meshNode = ni;
        Mat4 world = asset.nodes[static_cast<size_t>(ni)].local;
        int cur = asset.nodes[static_cast<size_t>(ni)].parent;
        int guard = 0;
        while (cur >= 0 && cur < nodeCount && guard++ <= nodeCount) {
            world = asset.nodes[static_cast<size_t>(cur)].local * world;
            cur = asset.nodes[static_cast<size_t>(cur)].parent;
        }
        out->meshNodeInverse = world.inverse();
        break;
    }

    out->sourceName = asset.generator.empty() ? "glb" : asset.generator;
    std::string verr;
    if (!out->validate(&verr)) return fail(verr.c_str());
    out->assignRoles();
    return true;
}

bool Skeleton::fromRig(const Rig& rig, Skeleton* out, std::string* error) {
    auto fail = [&](const char* msg) { if (error) *error = msg; return false; };
    if (!out) return fail("Skeleton::fromRig: null output");
    *out = Skeleton();
    const int n = rig.jointCount();
    if (n <= 0) return fail("the engine rig has no joints; run the rig stage first");
    const std::vector<Joint>& joints = rig.joints();

    out->names.reserve(static_cast<size_t>(n));
    out->parents.assign(static_cast<size_t>(n), -1);
    out->restT.assign(static_cast<size_t>(n), Vec3(0, 0, 0));
    out->restR.assign(static_cast<size_t>(n), Quat::identity());
    out->restS.assign(static_cast<size_t>(n), Vec3(1, 1, 1));
    out->jointToSkinSlot.assign(static_cast<size_t>(n), -1);
    out->sourceNode.assign(static_cast<size_t>(n), -1);
    out->skinSlotToJoint.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const Joint& j = joints[static_cast<size_t>(i)];
        out->names.push_back(j.name);
        out->parents[static_cast<size_t>(i)] = j.parent;
        out->restT[static_cast<size_t>(i)] = j.restTranslation;
        out->restR[static_cast<size_t>(i)] = j.restRotation;
        out->restS[static_cast<size_t>(i)] = j.restScale;
        out->jointToSkinSlot[static_cast<size_t>(i)] = i;
        out->skinSlotToJoint.push_back(i);
    }
    const std::vector<Mat4>& ib = rig.inverseBind();
    if (static_cast<int>(ib.size()) == n) {
        out->inverseBind = ib;
        out->hasInverseBind = true;
    }
    /* The generated rig is already in bind space and its mesh node carries no transform, so the
       skinning space is the identity. */
    out->meshNode = -1;
    out->meshNodeInverse = Mat4::identity();
    out->sourceName = "avatarforge-rig";

    out->order.clear();
    out->order.reserve(static_cast<size_t>(n));
    std::vector<char> placed(static_cast<size_t>(n), 0);
    bool progress = true;
    while (out->order.size() < static_cast<size_t>(n) && progress) {
        progress = false;
        for (int i = 0; i < n; ++i) {
            if (placed[static_cast<size_t>(i)]) continue;
            const int p = out->parents[static_cast<size_t>(i)];
            if (p >= 0 && (p >= n || !placed[static_cast<size_t>(p)])) continue;
            placed[static_cast<size_t>(i)] = 1;
            out->order.push_back(i);
            progress = true;
        }
    }
    if (out->order.size() != static_cast<size_t>(n)) {
        return fail("the engine rig's joint hierarchy contains a cycle");
    }
    std::string verr;
    if (!out->validate(&verr)) return fail(verr.c_str());
    out->assignRoles();
    return true;
}

bool Skeleton::fromGlbNodes(const GlbAsset& asset, Skeleton* out, std::string* error) {
    auto fail = [&](const char* msg) { if (error) *error = msg; return false; };
    if (!out) return fail("Skeleton::fromGlbNodes: null output");
    *out = Skeleton();
    const int n = static_cast<int>(asset.nodes.size());
    if (n == 0) return fail("the imported document has no nodes");

    out->names.reserve(static_cast<size_t>(n));
    out->parents.assign(static_cast<size_t>(n), -1);
    out->sourceNode.reserve(static_cast<size_t>(n));
    out->restT.assign(static_cast<size_t>(n), Vec3(0, 0, 0));
    out->restR.assign(static_cast<size_t>(n), Quat::identity());
    out->restS.assign(static_cast<size_t>(n), Vec3(1, 1, 1));
    out->jointToSkinSlot.assign(static_cast<size_t>(n), -1);
    out->inverseBind.assign(static_cast<size_t>(n), Mat4::identity());
    out->hasInverseBind = false;
    for (int i = 0; i < n; ++i) {
        const ImportedNode& node = asset.nodes[static_cast<size_t>(i)];
        out->names.push_back(node.name.empty() ? ("<node " + std::to_string(i) + ">") : node.name);
        out->parents[static_cast<size_t>(i)] = (node.parent >= 0 && node.parent < n) ? node.parent : -1;
        out->sourceNode.push_back(i);
        Vec3 t; Quat r; Vec3 s;
        if (!decomposeTrs(node.local, &t, &r, &s)) {
            return fail("a node's transform could not be decomposed into TRS");
        }
        out->restT[static_cast<size_t>(i)] = t;
        out->restR[static_cast<size_t>(i)] = r;
        out->restS[static_cast<size_t>(i)] = s;
    }
    out->order.clear();
    out->order.reserve(static_cast<size_t>(n));
    std::vector<char> placed(static_cast<size_t>(n), 0);
    bool progress = true;
    while (out->order.size() < static_cast<size_t>(n) && progress) {
        progress = false;
        for (int i = 0; i < n; ++i) {
            if (placed[static_cast<size_t>(i)]) continue;
            const int p = out->parents[static_cast<size_t>(i)];
            if (p >= 0 && !placed[static_cast<size_t>(p)]) continue;
            placed[static_cast<size_t>(i)] = 1;
            out->order.push_back(i);
            progress = true;
        }
    }
    if (out->order.size() != static_cast<size_t>(n)) return fail("the node hierarchy contains a cycle");
    out->meshNode = -1;
    out->meshNodeInverse = Mat4::identity();
    out->sourceName = asset.generator.empty() ? "glb-nodes" : (asset.generator + " (nodes only)");
    std::string verr;
    if (!out->validate(&verr)) return fail(verr.c_str());
    out->assignRoles();
    return true;
}

} /* namespace anim */
} /* namespace af */
