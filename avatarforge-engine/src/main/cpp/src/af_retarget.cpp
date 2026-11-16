/*
 * af_retarget.cpp -- see af_retarget.h for the method.
 */
#include "af_retarget.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace af {
namespace {

/* ------------------------------------------------------------------ names -> roles ----------- */

enum Role : int {
    kNone = -1,
    kHips = 0, kSpine, kNeck, kHead,
    kShoulder, kUpperArm, kForeArm, kHand,
    kThigh, kShin, kFoot, kToe,
    kThumb, kIndex, kMiddle, kRing, kPinky
};

struct NameInfo {
    Role role = kNone;
    int side = 0;      /* +1 the name says left, -1 it says right, 0 it says neither */
    int order = 0;     /* the number after the role word: Spine2 -> 2, thumb_01 -> 1 */
};

/* Splits a joint name into lower-case words: at anything that is not a letter or digit, where a
   lower-case letter is followed by a capital, and between letters and digits. A prefix up to the
   last ':' or '|' ("mixamorig:", "Armature|") is dropped first. */
std::vector<std::string> wordsOf(const std::string& raw) {
    const size_t cut = raw.find_last_of(":|");
    const std::string s = cut == std::string::npos ? raw : raw.substr(cut + 1);
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() { if (!cur.empty()) { out.push_back(cur); cur.clear(); } };
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const bool alpha = std::isalpha(c) != 0, digit = std::isdigit(c) != 0;
        if (!alpha && !digit) { flush(); continue; }
        if (!cur.empty()) {
            const unsigned char p = static_cast<unsigned char>(s[i - 1]);
            const bool prevDigit = std::isdigit(p) != 0;
            const bool nextLower = i + 1 < s.size() && std::islower(static_cast<unsigned char>(s[i + 1])) != 0;
            if (digit != prevDigit) flush();
            else if (alpha && std::isupper(c) && std::islower(p)) flush();
            else if (alpha && std::isupper(c) && std::isupper(p) && nextLower) flush();
        }
        cur.push_back(static_cast<char>(std::tolower(c)));
    }
    flush();
    return out;
}

bool isOneOf(const std::string& w, std::initializer_list<const char*> list) {
    for (const char* item : list) if (w == item) return true;
    return false;
}

NameInfo classify(const std::string& name) {
    NameInfo info;
    std::string key;
    for (const std::string& w : wordsOf(name)) {
        if (w == "left" || w == "l") { info.side = 1; continue; }
        if (w == "right" || w == "r") { info.side = -1; continue; }
        if (std::isdigit(static_cast<unsigned char>(w[0]))) {
            /* "Bip01 Spine1": only a number after the role word counts. */
            if (!key.empty()) info.order = std::atoi(w.c_str());
            continue;
        }
        /* End markers, helpers and parts this rig has no bone for: not a joint to follow. */
        if (isOneOf(w, {"end", "top", "nub", "tip", "twist", "roll", "ik", "pole", "target", "ctrl", "control",
                        "eye", "jaw", "tongue", "breast", "hair", "tail", "wing", "weapon", "prop", "socket"})) {
            return NameInfo{};
        }
        if (isOneOf(w, {"proximal"})) { info.order = 1; continue; }
        if (isOneOf(w, {"intermediate"})) { info.order = 2; continue; }
        if (isOneOf(w, {"distal"})) { info.order = 3; continue; }
        if (isOneOf(w, {"metacarpal"})) return NameInfo{};
        /* Words that are part of a naming scheme, not of the joint's meaning. */
        if (isOneOf(w, {"mixamorig", "armature", "bip", "biped", "def", "org", "bone", "joint", "jnt", "sk", "skel",
                        "skeleton", "rig", "cc", "base", "character", "finger"})) continue;
        if (w.size() == 1) continue;
        key += w;
    }
    struct Entry { const char* key; Role role; };
    static const Entry table[] = {
        {"hips", kHips}, {"hip", kHips}, {"pelvis", kHips},
        {"spine", kSpine}, {"chest", kSpine}, {"upperchest", kSpine}, {"lowerchest", kSpine}, {"abdomen", kSpine},
        {"torso", kSpine}, {"waist", kSpine}, {"spinelower", kSpine}, {"spineupper", kSpine},
        {"neck", kNeck}, {"head", kHead},
        {"shoulder", kShoulder}, {"clavicle", kShoulder}, {"collar", kShoulder}, {"collarbone", kShoulder},
        {"arm", kUpperArm}, {"upperarm", kUpperArm}, {"uparm", kUpperArm},
        {"forearm", kForeArm}, {"lowerarm", kForeArm},
        {"hand", kHand}, {"wrist", kHand},
        {"upleg", kThigh}, {"upperleg", kThigh}, {"thigh", kThigh},
        {"leg", kShin}, {"lowerleg", kShin}, {"calf", kShin}, {"shin", kShin},
        {"foot", kFoot}, {"ankle", kFoot},
        {"toe", kToe}, {"toes", kToe}, {"ball", kToe},
        {"handthumb", kThumb}, {"thumb", kThumb},
        {"handindex", kIndex}, {"index", kIndex},
        {"handmiddle", kMiddle}, {"middle", kMiddle},
        {"handring", kRing}, {"ring", kRing},
        {"handpinky", kPinky}, {"pinky", kPinky}, {"handlittle", kPinky}, {"little", kPinky},
    };
    for (const Entry& e : table) {
        if (key == e.key) { info.role = e.role; return info; }
    }
    return NameInfo{};
}

/* ------------------------------------------------------------------ small maths -------------- */

Quat quatOf(const Mat4& m) {
    Vec3 c0(m.m[0], m.m[1], m.m[2]), c1(m.m[4], m.m[5], m.m[6]), c2(m.m[8], m.m[9], m.m[10]);
    const float l0 = c0.length(), l1 = c1.length(), l2 = c2.length();
    if (l0 < 1e-12f || l1 < 1e-12f || l2 < 1e-12f) return Quat::identity();
    c0 = c0 / l0; c1 = c1 / l1; c2 = c2 / l2;
    const float r00 = c0.x, r10 = c0.y, r20 = c0.z, r01 = c1.x, r11 = c1.y, r21 = c1.z, r02 = c2.x, r12 = c2.y, r22 = c2.z;
    const float trace = r00 + r11 + r22;
    Quat q;
    if (trace > 0.0f) {
        const float k = std::sqrt(trace + 1.0f) * 2.0f;
        q = Quat((r21 - r12) / k, (r02 - r20) / k, (r10 - r01) / k, 0.25f * k);
    } else if (r00 > r11 && r00 > r22) {
        const float k = std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
        q = Quat(0.25f * k, (r01 + r10) / k, (r02 + r20) / k, (r21 - r12) / k);
    } else if (r11 > r22) {
        const float k = std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
        q = Quat((r01 + r10) / k, 0.25f * k, (r12 + r21) / k, (r02 - r20) / k);
    } else {
        const float k = std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
        q = Quat((r02 + r20) / k, (r12 + r21) / k, 0.25f * k, (r10 - r01) / k);
    }
    return q.normalized();
}

Vec3 positionOf(const Mat4& m) { return Vec3(m.m[12], m.m[13], m.m[14]); }
Vec3 scaleOf(const Mat4& m) {
    return Vec3(Vec3(m.m[0], m.m[1], m.m[2]).length(), Vec3(m.m[4], m.m[5], m.m[6]).length(),
                Vec3(m.m[8], m.m[9], m.m[10]).length());
}
Vec3 horizontal(const Vec3& v) { return Vec3(v.x, 0.0f, v.z); }

float angleBetween(const Quat& a, const Quat& b) {
    const float d = std::fabs(a.dot(b));
    return 2.0f * std::acos(clampf(d, 0.0f, 1.0f));
}

/* One channel at one time. Cubic-spline channels are read through their key values: the tangents
   shape the path between keys, and at 30 samples a second of a sampled source the difference is
   below what a retargeted pose can show. */
Vec4 sampleChannel(const ImportedAnimationChannel& c, float t, bool rotation) {
    const size_t n = std::min(c.times.size(), c.values.size());
    if (n == 0) return rotation ? Vec4(0, 0, 0, 1) : Vec4(0, 0, 0, 0);
    if (n == 1 || t <= c.times[0]) return c.values[0];
    if (t >= c.times[n - 1]) return c.values[n - 1];
    const size_t hi = static_cast<size_t>(std::upper_bound(c.times.begin(), c.times.begin() + static_cast<std::ptrdiff_t>(n), t) - c.times.begin());
    const size_t lo = hi - 1;
    if (c.interpolation == 1) return c.values[lo];
    const float span = c.times[hi] - c.times[lo];
    const float u = span > 1e-9f ? (t - c.times[lo]) / span : 0.0f;
    const Vec4& a = c.values[lo];
    const Vec4& b = c.values[hi];
    if (rotation) {
        const Quat q = Quat::slerp(Quat(a.x, a.y, a.z, a.w).normalized(), Quat(b.x, b.y, b.z, b.w).normalized(), u);
        return Vec4(q.x, q.y, q.z, q.w);
    }
    return Vec4(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u, 0.0f);
}

/* ------------------------------------------------------------------ the two skeletons -------- */

constexpr int kLimbRoles = 8;      /* shoulder, upper arm, forearm, hand, thigh, shin, foot, toe */
const Role kLimbRole[kLimbRoles] = {kShoulder, kUpperArm, kForeArm, kHand, kThigh, kShin, kFoot, kToe};
const char* const kLimbTarget[kLimbRoles] = {"clavicle", "upperarm_01", "forearm_01", "hand", "thigh", "shin", "foot", "toe"};
const char* const kLimbWord[kLimbRoles] = {"shoulder", "upper arm", "forearm", "hand", "thigh", "shin", "foot", "toe"};
const Role kFingerRole[5] = {kThumb, kIndex, kMiddle, kRing, kPinky};
const char* const kFingerTarget[5] = {"thumb", "index", "middle", "ring", "pinky"};

struct SourceSkeleton {
    int hips = -1, head = -1;
    std::vector<int> spine, neck;                 /* node indices, nearest the hips first */
    int limb[2][kLimbRoles];                      /* [0 = the name says left, 1 = right] */
    int finger[2][5][3];
    SourceSkeleton() {
        for (auto& side : limb) for (int& n : side) n = -1;
        for (auto& side : finger) for (auto& f : side) for (int& n : f) n = -1;
    }
};

struct Pair {
    int source = -1;       /* node of the file */
    int target = -1;       /* joint of this rig */
    int nextSource = -1;   /* the joint each bone points at, for the rest-pose alignment */
    int nextTarget = -1;
    Quat align = Quat::identity();
};

int depthOf(const GlbAsset& asset, int node) {
    int d = 0;
    for (int n = node; n >= 0 && d < 4096; n = asset.nodes[static_cast<size_t>(n)].parent) ++d;
    return d;
}

} // namespace

bool retargetAnimations(const GlbAsset& source, const Rig& target, float sampleHz,
                        std::vector<RetargetedClip>* outClips, RetargetReport* report) {
    RetargetReport local;
    RetargetReport& rep = report ? *report : local;
    rep = RetargetReport{};
    if (outClips) outClips->clear();
    rep.animationsInFile = static_cast<int>(source.animations.size());
    const size_t N = source.nodes.size();
    const int J = target.jointCount();
    if (N == 0 || J == 0) {
        rep.message = N == 0 ? "The file has no nodes." : "This model has no skeleton to move.";
        return false;
    }

    /* ---- the file's joints: those of its first real skin, or every node when it has none ---- */
    std::vector<char> isJoint(N, 0);
    {
        const ImportedSkin* skin = nullptr;
        for (const ImportedSkin& s : source.skins) if (s.joints.size() >= 5) { skin = &s; break; }
        if (skin) {
            for (int n : skin->joints) if (n >= 0 && static_cast<size_t>(n) < N) isJoint[static_cast<size_t>(n)] = 1;
        } else {
            std::fill(isJoint.begin(), isJoint.end(), 1);
        }
    }
    for (char j : isJoint) rep.sourceJoints += j ? 1 : 0;

    /* ---- recognise them by name ---- */
    SourceSkeleton src;
    for (size_t n = 0; n < N; ++n) {
        if (!isJoint[n]) continue;
        const NameInfo info = classify(source.nodes[n].name);
        if (info.role == kNone) continue;
        const int node = static_cast<int>(n);
        bool used = false;
        if (info.role == kHips) { if (src.hips < 0) { src.hips = node; used = true; } }
        else if (info.role == kHead) { if (src.head < 0) { src.head = node; used = true; } }
        else if (info.role == kSpine) { src.spine.push_back(node); used = true; }
        else if (info.role == kNeck) { src.neck.push_back(node); used = true; }
        else if (info.side != 0) {
            const int side = info.side > 0 ? 0 : 1;
            for (int r = 0; r < kLimbRoles; ++r) {
                if (kLimbRole[r] == info.role && src.limb[side][r] < 0) { src.limb[side][r] = node; used = true; }
            }
            for (int f = 0; f < 5; ++f) {
                if (kFingerRole[f] != info.role) continue;
                /* Thumb1..3 and thumb_01..03 both start at 1; a file that starts at 0 is shifted. */
                const int k = info.order - 1;
                if (k >= 0 && k < 3 && src.finger[side][f][k] < 0) { src.finger[side][f][k] = node; used = true; }
            }
        }
        if (used) ++rep.recognisedJoints;
    }
    auto byDepth = [&](int a, int b) { return depthOf(source, a) < depthOf(source, b); };
    std::stable_sort(src.spine.begin(), src.spine.end(), byDepth);
    std::stable_sort(src.neck.begin(), src.neck.end(), byDepth);

    /* ---- is it a humanoid this rig can follow? ---- */
    {
        std::string missing;
        auto need = [&](bool present, const std::string& what) {
            if (present) return;
            if (!missing.empty()) missing += ", ";
            missing += what;
        };
        need(src.hips >= 0, "hips");
        need(!src.spine.empty(), "spine");
        need(src.head >= 0 || !src.neck.empty(), "head or neck");
        const int required[4] = {1, 2, 4, 5};   /* upper arm, forearm, thigh, shin */
        for (int side = 0; side < 2; ++side) {
            for (int r : required) need(src.limb[side][r] >= 0, std::string(side == 0 ? "left " : "right ") + kLimbWord[r]);
        }
        if (!missing.empty()) {
            rep.message = "This file's skeleton cannot drive the avatar. It has " + std::to_string(rep.sourceJoints) +
                          " joints, " + std::to_string(rep.recognisedJoints) + " recognised by name; missing: " + missing + ".";
            return false;
        }
    }
    if (source.animations.empty()) {
        rep.compatible = true;
        rep.message = "The skeleton fits, but the file has no animation.";
        return false;
    }

    /* ---- rest pose of the file, in world space ---- */
    std::vector<Vec3> restPos(N);
    std::vector<Quat> restRot(N);
    for (size_t n = 0; n < N; ++n) {
        restPos[n] = positionOf(source.nodes[n].world);
        restRot[n] = quatOf(source.nodes[n].world);
    }

    /* ---- where the file's figure faces, and which of its sides is which ---- */
    const Vec3 up(0, 1, 0);
    Vec3 forward(0, 0, 0);
    for (int side = 0; side < 2; ++side) {
        const int foot = src.limb[side][6], toe = src.limb[side][7];
        if (foot >= 0 && toe >= 0) forward += horizontal(restPos[static_cast<size_t>(toe)] - restPos[static_cast<size_t>(foot)]);
    }
    bool facingFromToes = forward.length() > 1e-4f;
    if (!facingFromToes) {
        /* No toes: trust the names. For a figure whose left is on the left, forward = left x up. */
        const Vec3 across = horizontal(restPos[static_cast<size_t>(src.limb[0][1])] - restPos[static_cast<size_t>(src.limb[1][1])]);
        forward = across.cross(up);
    }
    forward = horizontal(forward).normalizedOr(Vec3(0, 0, 1));
    const Vec3 leftDir = up.cross(forward);
    /* Do the names "left" and "right" mean the figure's own left and right? */
    int votes = 0;
    for (int r = 0; r < kLimbRoles; ++r) {
        const int a = src.limb[0][r], b = src.limb[1][r];
        if (a < 0 || b < 0) continue;
        votes += (restPos[static_cast<size_t>(a)] - restPos[static_cast<size_t>(b)]).dot(leftDir) > 0.0f ? 1 : -1;
    }
    const bool namesAreAnatomical = votes >= 0;
    /* sourceSide[0] is the figure's own left, whatever the file calls it. */
    const int sourceSide[2] = {namesAreAnatomical ? 0 : 1, namesAreAnatomical ? 1 : 0};

    /* This rig faces -Z, so its own left is -X. Which of its suffixes is on that side? */
    const std::vector<Joint>& tj = target.joints();
    const int probeL = target.jointIndex("upperarm_01_L"), probeR = target.jointIndex("upperarm_01_R");
    if (probeL < 0 || probeR < 0 || target.jointIndex("hips") < 0) {
        rep.message = "This model's skeleton is not an AvatarForge rig.";
        return false;
    }
    const bool lIsLeft = tj[static_cast<size_t>(probeL)].worldPosition.x < tj[static_cast<size_t>(probeR)].worldPosition.x;
    const char* const targetSuffix[2] = {lIsLeft ? "_L" : "_R", lIsLeft ? "_R" : "_L"};

    /* The turn about the vertical that makes the file's figure face the way this rig does. */
    const float yawSource = std::atan2(forward.x, forward.z);
    const float yawTarget = std::atan2(0.0f, -1.0f);
    const Quat turn = Quat::fromAxisAngle(up, yawTarget - yawSource);
    const Quat turnBack = turn.conjugate();

    /* ---- pair the joints ---- */
    std::vector<Pair> pairs;
    std::vector<int> pairOfTarget(static_cast<size_t>(J), -1);
    auto addPair = [&](int sourceNode, const std::string& targetName) -> int {
        if (sourceNode < 0) return -1;
        const int t = target.jointIndex(targetName.c_str());
        if (t < 0 || pairOfTarget[static_cast<size_t>(t)] >= 0) return -1;
        Pair p;
        p.source = sourceNode;
        p.target = t;
        pairOfTarget[static_cast<size_t>(t)] = static_cast<int>(pairs.size());
        pairs.push_back(p);
        return static_cast<int>(pairs.size()) - 1;
    };
    auto pointAt = [&](int pair, int nextPair) {
        if (pair < 0 || nextPair < 0) return;
        pairs[static_cast<size_t>(pair)].nextSource = pairs[static_cast<size_t>(nextPair)].source;
        pairs[static_cast<size_t>(pair)].nextTarget = pairs[static_cast<size_t>(nextPair)].target;
    };

    addPair(src.hips, "hips");
    /* Spine: this rig has five joints from the hips to the chest; spread the file's over them. */
    {
        std::vector<int> chain;
        static const char* const kSpineTarget[5] = {"spine_01", "spine_02", "spine_03", "spine_04", "chest"};
        const int n = static_cast<int>(src.spine.size());
        if (n == 1) {
            chain.push_back(addPair(src.spine[0], kSpineTarget[2]));
        } else if (n <= 5) {
            for (int i = 0; i < n; ++i) {
                const int slot = static_cast<int>(std::lround(static_cast<float>(i) * 4.0f / static_cast<float>(n - 1)));
                chain.push_back(addPair(src.spine[static_cast<size_t>(i)], kSpineTarget[slot]));
            }
        } else {
            for (int k = 0; k < 5; ++k) {
                const int i = static_cast<int>(std::lround(static_cast<float>(k) * static_cast<float>(n - 1) / 4.0f));
                chain.push_back(addPair(src.spine[static_cast<size_t>(i)], kSpineTarget[k]));
            }
        }
        if (!src.neck.empty()) {
            chain.push_back(addPair(src.neck.front(), "neck_01"));
            if (src.neck.size() > 1) chain.push_back(addPair(src.neck.back(), "neck_02"));
        }
        chain.push_back(addPair(src.head, "head"));
        /* No rest-pose alignment along the trunk. Both figures stand upright at rest, and where a
           rig puts its spine, neck and head joints inside the body is a matter of taste: turning
           this rig's neck-to-head line onto the file's tipped the head back by tens of degrees.
           These joints take the file's rotation since rest as it is. */
    }
    for (int side = 0; side < 2; ++side) {
        const int from = sourceSide[side];
        const std::string suffix = targetSuffix[side];
        int limbPair[kLimbRoles];
        for (int r = 0; r < kLimbRoles; ++r) limbPair[r] = addPair(src.limb[from][r], std::string(kLimbTarget[r]) + suffix);
        /* The clavicle is left unaligned for the same reason as the trunk. */
        pointAt(limbPair[1], limbPair[2]);
        pointAt(limbPair[2], limbPair[3]);
        pointAt(limbPair[4], limbPair[5]);
        pointAt(limbPair[5], limbPair[6]);
        pointAt(limbPair[6], limbPair[7]);
        int middleBase = -1;
        for (int f = 0; f < 5; ++f) {
            int fingerPair[3];
            for (int k = 0; k < 3; ++k) {
                char name[32];
                std::snprintf(name, sizeof(name), "%s_%02d%s", kFingerTarget[f], k + 1, suffix.c_str());
                /* A finger is followed only from its first joint on: a second joint without the
                   first would bend from the wrong place. */
                const bool chainIntact = k == 0 || fingerPair[k - 1] >= 0;
                fingerPair[k] = (limbPair[3] >= 0 && chainIntact) ? addPair(src.finger[from][f][k], name) : -1;
            }
            pointAt(fingerPair[0], fingerPair[1]);
            pointAt(fingerPair[1], fingerPair[2]);
            if (f == 2) middleBase = fingerPair[0];
        }
        pointAt(limbPair[3], middleBase);      /* the hand points at the middle finger */
    }
    rep.mappedJoints = static_cast<int>(pairs.size());

    /* ---- rest-pose alignment: this rig's bone turned onto the file's bone ---- */
    for (Pair& p : pairs) {
        if (p.nextSource < 0 || p.nextTarget < 0) continue;
        const Vec3 dirTarget = tj[static_cast<size_t>(p.nextTarget)].worldPosition - tj[static_cast<size_t>(p.target)].worldPosition;
        const Vec3 dirSource = turn.rotate(restPos[static_cast<size_t>(p.nextSource)] - restPos[static_cast<size_t>(p.source)]);
        if (dirTarget.length() < 1e-5f || dirSource.length() < 1e-7f) continue;
        p.align = Quat::fromTo(dirTarget, dirSource);
    }
    /* The end of a limb has nothing to point at (a toe, a finger tip, a hand without fingers). It
       keeps the alignment of the paired joint above it, so that it stays as it is on its parent. */
    for (int j = 0; j < J; ++j) {
        const int pi = pairOfTarget[static_cast<size_t>(j)];
        if (pi < 0) continue;
        Pair& p = pairs[static_cast<size_t>(pi)];
        if (p.nextTarget >= 0) continue;
        const std::string& name = tj[static_cast<size_t>(j)].name;
        const bool endOfLimb = name.compare(0, 4, "hand") == 0 || name.compare(0, 4, "foot") == 0 || name.compare(0, 3, "toe") == 0 ||
                               name.compare(0, 5, "thumb") == 0 || name.compare(0, 5, "index") == 0 || name.compare(0, 6, "middle") == 0 ||
                               name.compare(0, 4, "ring") == 0 || name.compare(0, 5, "pinky") == 0;
        if (!endOfLimb) continue;
        for (int a = tj[static_cast<size_t>(j)].parent; a >= 0; a = tj[static_cast<size_t>(a)].parent) {
            const int api = pairOfTarget[static_cast<size_t>(a)];
            if (api >= 0) { p.align = pairs[static_cast<size_t>(api)].align; break; }
        }
    }

    /* ---- how much bigger or smaller this figure is: ratio of the leg lengths ---- */
    float scale = 1.0f;
    {
        auto legLength = [&](int thigh, int shin, int foot, const std::vector<Vec3>& pos) {
            if (thigh < 0 || shin < 0) return 0.0f;
            float len = (pos[static_cast<size_t>(shin)] - pos[static_cast<size_t>(thigh)]).length();
            if (foot >= 0) len += (pos[static_cast<size_t>(foot)] - pos[static_cast<size_t>(shin)]).length();
            return len;
        };
        std::vector<Vec3> targetPos(static_cast<size_t>(J));
        for (int j = 0; j < J; ++j) targetPos[static_cast<size_t>(j)] = tj[static_cast<size_t>(j)].worldPosition;
        const std::string suffix = targetSuffix[0];
        const int from = sourceSide[0];
        const float sourceLeg = legLength(src.limb[from][4], src.limb[from][5], src.limb[from][6], restPos);
        const float targetLeg = legLength(target.jointIndex(("thigh" + suffix).c_str()), target.jointIndex(("shin" + suffix).c_str()),
                                          src.limb[from][6] >= 0 ? target.jointIndex(("foot" + suffix).c_str()) : -1, targetPos);
        if (sourceLeg > 1e-6f && targetLeg > 1e-6f) scale = targetLeg / sourceLeg;
    }

    /* ---- evaluation order of the file's nodes: parents first ---- */
    std::vector<int> order;
    order.reserve(N);
    {
        std::vector<char> done(N, 0);
        for (size_t pass = 0; pass < N && order.size() < N; ++pass) {
            bool progressed = false;
            for (size_t n = 0; n < N; ++n) {
                if (done[n]) continue;
                const int parent = source.nodes[n].parent;
                if (parent >= 0 && (static_cast<size_t>(parent) >= N || !done[static_cast<size_t>(parent)])) continue;
                done[n] = 1;
                order.push_back(static_cast<int>(n));
                progressed = true;
            }
            if (!progressed) break;     /* a cycle: the importer rejects those, but do not spin */
        }
    }
    std::vector<Vec3> restT(N), restS(N);
    std::vector<Quat> restR(N);
    for (size_t n = 0; n < N; ++n) {
        restT[n] = positionOf(source.nodes[n].local);
        restS[n] = scaleOf(source.nodes[n].local);
        restR[n] = quatOf(source.nodes[n].local);
    }

    const float hz = (std::isfinite(sampleHz) && sampleHz >= 5.0f && sampleHz <= 120.0f) ? sampleHz : 30.0f;
    const int hipsTarget = target.jointIndex("hips");
    const int hipsPair = pairOfTarget[static_cast<size_t>(hipsTarget)];
    std::vector<const ImportedAnimationChannel*> chT(N), chR(N), chS(N);
    std::vector<Mat4> world(N);
    std::vector<Quat> worldRot(static_cast<size_t>(J));
    int skippedEmpty = 0, travelling = 0;

    for (const ImportedAnimation& anim : source.animations) {
        if (outClips && outClips->size() >= 128) break;
        std::fill(chT.begin(), chT.end(), nullptr);
        std::fill(chR.begin(), chR.end(), nullptr);
        std::fill(chS.begin(), chS.end(), nullptr);
        float duration = std::isfinite(anim.duration) ? anim.duration : 0.0f;
        bool movesAPairedJoint = false;
        for (const ImportedAnimationChannel& c : anim.channels) {
            if (c.node < 0 || static_cast<size_t>(c.node) >= N || c.times.empty() || c.values.empty()) continue;
            if (c.path == 0) chT[static_cast<size_t>(c.node)] = &c;
            else if (c.path == 1) chR[static_cast<size_t>(c.node)] = &c;
            else if (c.path == 2) chS[static_cast<size_t>(c.node)] = &c;
            duration = std::max(duration, c.times.back());
            if (c.path == 1) for (const Pair& p : pairs) if (p.source == c.node) { movesAPairedJoint = true; break; }
        }
        if (!movesAPairedJoint) { ++skippedEmpty; continue; }
        duration = std::min(duration, 180.0f);
        const bool still = duration < 1e-3f;
        if (still) duration = 1.0f;       /* a single pose: hold it for a second */
        const int frames = std::max(2, static_cast<int>(std::ceil(duration * hz)) + 1);

        RetargetedClip clip;
        clip.name = anim.name;
        clip.duration = duration;
        clip.frames = frames;
        clip.quats.assign(static_cast<size_t>(frames) * static_cast<size_t>(J) * 4, 0.0f);
        std::vector<Vec3> hipsTravel(static_cast<size_t>(frames), Vec3(0, 0, 0));

        for (int f = 0; f < frames; ++f) {
            const float t = still ? 0.0f : duration * static_cast<float>(f) / static_cast<float>(frames - 1);
            for (int n : order) {
                const size_t i = static_cast<size_t>(n);
                Vec3 tr = restT[i], sc = restS[i];
                Quat ro = restR[i];
                if (chT[i]) { const Vec4 v = sampleChannel(*chT[i], t, false); tr = Vec3(v.x, v.y, v.z); }
                if (chR[i]) { const Vec4 v = sampleChannel(*chR[i], t, true); ro = Quat(v.x, v.y, v.z, v.w).normalized(); }
                if (chS[i]) { const Vec4 v = sampleChannel(*chS[i], t, false); sc = Vec3(v.x, v.y, v.z); }
                const Mat4 localM = Mat4::trs(tr, ro, sc);
                const int parent = source.nodes[i].parent;
                world[i] = parent >= 0 ? world[static_cast<size_t>(parent)] * localM : localM;
            }
            float* q = &clip.quats[static_cast<size_t>(f) * static_cast<size_t>(J) * 4];
            for (int j = 0; j < J; ++j) {
                const int parent = tj[static_cast<size_t>(j)].parent;
                const Quat parentRot = parent >= 0 ? worldRot[static_cast<size_t>(parent)] : Quat::identity();
                const int pi = pairOfTarget[static_cast<size_t>(j)];
                Quat rot = parentRot;      /* unpaired: no rotation of its own */
                if (pi >= 0) {
                    const Pair& p = pairs[static_cast<size_t>(pi)];
                    const Quat now = quatOf(world[static_cast<size_t>(p.source)]);
                    const Quat sinceRest = turn * now * restRot[static_cast<size_t>(p.source)].conjugate() * turnBack;
                    rot = (sinceRest * p.align).normalized();
                }
                worldRot[static_cast<size_t>(j)] = rot;
                const Quat localRot = (parentRot.conjugate() * rot).normalized();
                q[static_cast<size_t>(j) * 4 + 0] = localRot.x;
                q[static_cast<size_t>(j) * 4 + 1] = localRot.y;
                q[static_cast<size_t>(j) * 4 + 2] = localRot.z;
                q[static_cast<size_t>(j) * 4 + 3] = localRot.w;
            }
            if (hipsPair >= 0) {
                const int hipsNode = pairs[static_cast<size_t>(hipsPair)].source;
                hipsTravel[static_cast<size_t>(f)] =
                    turn.rotate(positionOf(world[static_cast<size_t>(hipsNode)]) - restPos[static_cast<size_t>(hipsNode)]) * scale;
            }
        }

        /* Keep neighbouring frames on the same side of the quaternion sphere, so that blending
           two frames never takes the long way round. */
        for (int f = 1; f < frames; ++f) {
            float* prev = &clip.quats[static_cast<size_t>(f - 1) * static_cast<size_t>(J) * 4];
            float* cur = &clip.quats[static_cast<size_t>(f) * static_cast<size_t>(J) * 4];
            for (int j = 0; j < J; ++j) {
                const float* a = prev + static_cast<size_t>(j) * 4;
                float* b = cur + static_cast<size_t>(j) * 4;
                if (a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3] < 0.0f) {
                    b[0] = -b[0]; b[1] = -b[1]; b[2] = -b[2]; b[3] = -b[3];
                }
            }
        }

        /* Travel: a walk that leaves the spot is played on the spot. What is taken out is the
           steady drift between the first and the last frame, so sway and bounce stay. */
        const Vec3 drift = horizontal(hipsTravel.back() - hipsTravel.front());
        if (!still && drift.length() > 0.25f) {
            clip.travels = true;
            ++travelling;
            for (int f = 0; f < frames; ++f) {
                hipsTravel[static_cast<size_t>(f)] -= drift * (static_cast<float>(f) / static_cast<float>(frames - 1));
            }
        }
        bool anyOffset = false;
        for (const Vec3& v : hipsTravel) if (v.length() > 1e-5f) { anyOffset = true; break; }
        if (anyOffset) {
            clip.offsets.assign(static_cast<size_t>(frames) * static_cast<size_t>(J) * 3, 0.0f);
            for (int f = 0; f < frames; ++f) {
                float* o = &clip.offsets[(static_cast<size_t>(f) * static_cast<size_t>(J) + static_cast<size_t>(hipsTarget)) * 3];
                o[0] = hipsTravel[static_cast<size_t>(f)].x;
                o[1] = hipsTravel[static_cast<size_t>(f)].y;
                o[2] = hipsTravel[static_cast<size_t>(f)].z;
            }
        }

        /* Does it loop? The last pose has to be the first again, joint by joint and at the hips. */
        if (!still) {
            float worst = 0.0f;
            const float* first = clip.quats.data();
            const float* last = &clip.quats[static_cast<size_t>(frames - 1) * static_cast<size_t>(J) * 4];
            for (int j = 0; j < J; ++j) {
                const Quat a(first[j * 4], first[j * 4 + 1], first[j * 4 + 2], first[j * 4 + 3]);
                const Quat b(last[j * 4], last[j * 4 + 1], last[j * 4 + 2], last[j * 4 + 3]);
                worst = std::max(worst, angleBetween(a, b));
            }
            const float hipsGap = (hipsTravel.back() - hipsTravel.front()).length();
            clip.looping = worst < radians(9.0f) && hipsGap < 0.05f;
        }
        if (outClips) outClips->push_back(std::move(clip));
        ++rep.animationsAdded;
    }

    rep.compatible = true;
    if (rep.animationsAdded == 0) {
        rep.message = "The skeleton fits, but none of the file's " + std::to_string(rep.animationsInFile) +
                      " animation(s) moves a joint this avatar has.";
        return false;
    }
    rep.message = std::to_string(rep.animationsAdded) + " animation(s) added. " + std::to_string(rep.mappedJoints) + " of this avatar's " +
                  std::to_string(J) + " joints follow the file's " + std::to_string(rep.sourceJoints) + " (" +
                  std::to_string(rep.recognisedJoints) + " recognised by name). Size ratio " +
                  std::to_string(static_cast<int>(std::lround(scale * 100.0f))) + " %.";
    if (!facingFromToes) rep.message += " The file has no toes, so its facing was taken from the names of its sides.";
    if (!namesAreAnatomical) rep.message += " The file's left and right names are mirrored; sides were matched by position.";
    if (travelling > 0) rep.message += " " + std::to_string(travelling) + " travelling animation(s) play on the spot.";
    if (skippedEmpty > 0) rep.message += " " + std::to_string(skippedEmpty) + " animation(s) of the file moved none of these joints and were left out.";
    return true;
}

} // namespace af
