/*
 * af_rig.cpp -- skeleton construction, skin weighting, facial shapes and the clip library.
 */
#include "af_rig.h"
#include <cstring>
#include "af_threads.h"
#include "../include/afcore.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace af {

const char* const kBlendshapeNames[52] = {
    "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight",
    "cheekPuff", "cheekSquintLeft", "cheekSquintRight",
    "eyeBlinkLeft", "eyeBlinkRight", "eyeLookDownLeft", "eyeLookDownRight",
    "eyeLookInLeft", "eyeLookInRight", "eyeLookOutLeft", "eyeLookOutRight",
    "eyeLookUpLeft", "eyeLookUpRight", "eyeSquintLeft", "eyeSquintRight",
    "eyeWideLeft", "eyeWideRight",
    "jawForward", "jawLeft", "jawOpen", "jawRight",
    "mouthClose", "mouthDimpleLeft", "mouthDimpleRight", "mouthFrownLeft", "mouthFrownRight",
    "mouthFunnel", "mouthLeft", "mouthLowerDownLeft", "mouthLowerDownRight",
    "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight",
    "mouthRollLower", "mouthRollUpper", "mouthShrugLower", "mouthShrugUpper",
    "mouthSmileLeft", "mouthSmileRight", "mouthStretchLeft", "mouthStretchRight",
    "mouthUpperUpLeft", "mouthUpperUpRight", "noseSneerLeft", "noseSneerRight",
    "tongueOut"
};

/* ==============================================================================================
 * Skeleton
 * ============================================================================================== */
namespace {

/* Finger joint suffixes in anatomical order. */
const char* const kFingerChains[5][3] = {
    {"thumb_01", "thumb_02", "thumb_03"},
    {"index_01", "index_02", "index_03"},
    {"middle_01", "middle_02", "middle_03"},
    {"ring_01", "ring_02", "ring_03"},
    {"pinky_01", "pinky_02", "pinky_03"}
};
const char* const kFingerNames[5] = {"thumb", "index", "middle", "ring", "pinky"};

} // namespace

void Rig::finaliseRestPose() {
    /* Fills worldPosition for every joint from the local rest transforms, and builds the
       inverse bind matrices used by the GLB skin. */
    const int n = jointCount();
    inverseBind_.assign(n, Mat4::identity());
    for (int i = 0; i < n; ++i) {
        const Joint& j = joints_[i];
        const Mat4 local = Mat4::trs(j.restTranslation, j.restRotation, j.restScale);
        Mat4 world = local;
        if (j.parent >= 0) {
            const Vec3 pp = joints_[j.parent].worldPosition;
            world = Mat4::translation(pp) * local;
        }
        joints_[i].worldPosition = world.transformPoint(Vec3(0, 0, 0));
        inverseBind_[i] = world.inverse();
    }
}

float Rig::motionScale() const {
    /* Root translations of the clip library are written for a leg of 0.86 m, hip joint to ankle. */
    const int thigh = jointIndex("thigh_L"), shin = jointIndex("shin_L"), foot = jointIndex("foot_L");
    if (thigh < 0 || shin < 0 || foot < 0) return 1.0f;
    const float leg = (joints_[static_cast<size_t>(shin)].worldPosition - joints_[static_cast<size_t>(thigh)].worldPosition).length() +
                      (joints_[static_cast<size_t>(foot)].worldPosition - joints_[static_cast<size_t>(shin)].worldPosition).length();
    if (!(leg > 0.2f && leg < 3.0f)) return 1.0f;
    return leg / 0.86f;
}

int Rig::jointIndex(const char* name) const {
    for (size_t i = 0; i < joints_.size(); ++i) if (joints_[i].name == name) return static_cast<int>(i);
    return -1;
}

bool Rig::build(const BodyField& field, const Mesh& mesh) {
    field_ = &field;
    joints_.clear();
    buildSkeleton(field);
    if (joints_.empty()) return false;
    finaliseRestPose();
    computeWeights(field, mesh);
    buildBlendshapes(field, mesh);
    return true;
}

void Rig::buildSkeleton(const BodyField& field) {
    const BodyMetrics& m = field.metrics();
    auto add = [&](const std::string& name, int parent, const Vec3& worldPos, float radius,
                   int flags = 0, int region = REGION_SKIN) {
        Joint j;
        j.name = name;
        j.parent = parent;
        j.worldPosition = worldPos;
        j.radius = radius;
        j.flags = flags;
        j.bodyRegion = region;
        /* Local translation is filled in finaliseRestPose() from the parent's world position;
           storing it here as a delta keeps the hierarchy explicit and GLB-ready. */
        j.restTranslation = (parent >= 0)
                                ? (worldPos - joints_[static_cast<size_t>(parent)].worldPosition)
                                : worldPos;
        joints_.push_back(j);
        return static_cast<int>(joints_.size()) - 1;
    };

    const float pelvis = m.y.hipY - m.hip.rz * 0.10f;
    const int root = add("root", -1, Vec3(0, 0, 0), 0.05f);
    const int hips = add("hips", root, Vec3(0, pelvis, m.hip.centreZ), m.hip.rx * 0.62f);

    /* Spine chain: five segments between pelvis and neck base, with the natural curvature
       baked into the rest positions so the bind pose matches the mesh. */
    const int spine1 = add("spine_01", hips, Vec3(0, lerpf(pelvis, m.y.waistY, 0.35f), m.waist.centreZ - m.hip.rz * 0.02f), m.waist.rx * 0.75f);
    const int spine2 = add("spine_02", spine1, Vec3(0, m.y.waistY, m.waist.centreZ), m.waist.rx * 0.80f);
    const int spine3 = add("spine_03", spine2, Vec3(0, lerpf(m.y.waistY, m.y.underbustY, 0.55f), m.underbust.centreZ), m.underbust.rx * 0.82f);
    const int spine4 = add("spine_04", spine3, Vec3(0, lerpf(m.y.underbustY, m.y.armpitY, 0.60f), m.chest.centreZ * 0.9f), m.chest.rx * 0.80f);
    const int chest = add("chest", spine4, Vec3(0, m.y.armpitY, m.chest.centreZ), m.chest.rx * 0.85f);
    const int neck = add("neck_01", chest, Vec3(0, lerpf(m.y.neckBaseY, m.y.jawY, 0.35f), m.neck.centreZ), m.neck.rx * 1.15f);
    const int neck2 = add("neck_02", neck, Vec3(0, m.y.jawY - (m.y.crownY - m.y.jawY) * 0.045f, m.neck.centreZ * 0.5f), m.neck.rx * 1.35f);
    const int head = add("head", neck2, Vec3(0, m.y.jawY + (m.y.crownY - m.y.jawY) * 0.42f, 0), m.head.rx * 1.6f);
    const int jaw = add("jaw", head, Vec3(0, m.y.chinY + (m.y.crownY - m.y.chinY) * 0.06f, -m.head.rz * 0.45f), m.head.rx * 0.75f, AF_JOINT_FACE);
    add("eye_L", head, Vec3(m.eyeSeparation * 0.5f, m.y.eyeY, -m.head.rz * 0.78f), m.eyeRadius * 2.0f, AF_JOINT_FACE);
    add("eye_R", head, Vec3(-m.eyeSeparation * 0.5f, m.y.eyeY, -m.head.rz * 0.78f), m.eyeRadius * 2.0f, AF_JOINT_FACE);

    /* Arms */
    for (int s = 0; s < 2; ++s) {
        const int side = (s == 0) ? 1 : -1;
        const Chain& c = field.chain(side);
        const char* suffix = (side > 0) ? "L" : "R";
        const std::string sfx = std::string("_") + suffix;

        const int clav = add("clavicle" + sfx, chest,
                             Vec3(static_cast<float>(side) * m.neck.rx * 0.6f, m.y.neckBaseY - 0.006f, 0.0f),
                             m.biacromialHalf * 0.5f);
        const int shoulder = add("upperarm_01" + sfx, clav, c.shoulder, m.deltoidR * 1.15f);
        const int upperMid = add("upperarm_02" + sfx, shoulder,
                                 c.shoulder + (c.elbow - c.shoulder) * 0.52f, m.upperArmR * 1.1f);
        const int elbow = add("forearm_01" + sfx, upperMid, c.elbow, m.elbowR * 1.15f);
        const int foreMid = add("forearm_02" + sfx, elbow,
                                c.elbow + (c.wrist - c.elbow) * 0.55f, m.forearmR * 1.1f);
        const int wrist = add("hand" + sfx, foreMid, c.wrist, m.wristR * 1.6f);
        /* Twist bones let the forearm and upper arm roll without candy-wrapper collapse. */
        add("upperarm_twist" + sfx, shoulder, c.shoulder + (c.elbow - c.shoulder) * 0.38f, m.upperArmR * 0.65f, AF_JOINT_TWIST);
        add("forearm_twist" + sfx, elbow, c.elbow + (c.wrist - c.elbow) * 0.42f, m.forearmR * 0.65f, AF_JOINT_TWIST);

        for (int f = 0; f < 5; ++f) {
            int parent = wrist;
            /* The thumb has a real metacarpal segment (thumb_00) that carries it clear of the
               palm; the other fingers start at their MCP joint. */
            if (f == 0) {
                const Vec3 metacarpal = c.wrist.lerp(c.fingerJoints[0][0], 0.55f);
                parent = add("thumb_00" + sfx, wrist, metacarpal, m.fingerRadius[0] * 1.30f, AF_JOINT_FINGER);
            }
            for (int seg = 0; seg < 3; ++seg) {
                const Vec3 pos = c.fingerJoints[f][seg];
                const std::string jname = std::string(kFingerNames[f]) + "_" + std::string(1, '0') +
                                          std::string(1, static_cast<char>('1' + seg)) + sfx;
                (void)kFingerChains;
                parent = add(jname, parent, pos, m.fingerRadius[f] * (1.35f - 0.15f * static_cast<float>(seg)),
                             AF_JOINT_FINGER);
            }
        }
    }

    /* Legs */
    for (int s = 0; s < 2; ++s) {
        const int side = (s == 0) ? 1 : -1;
        const Chain& c = field.chain(side);
        const std::string sfx = (side > 0) ? "_L" : "_R";

        const int thigh = add("thigh" + sfx, hips, c.hipJoint, m.thighRx * 1.05f);
        const int knee = add("shin" + sfx, thigh, c.knee, m.kneeR * 1.20f);
        const int ankle = add("foot" + sfx, knee, c.ankle, m.ankleRx * 1.75f);
        add("toe" + sfx, ankle, c.ball, m.footWidth * 0.36f);
        add("thigh_twist" + sfx, thigh, c.hipJoint + (c.knee - c.hipJoint) * 0.45f, m.thighRx * 0.62f, AF_JOINT_TWIST);
    }
    (void)spine2; (void)spine3; (void)spine4; (void)chest; (void)neck; (void)neck2; (void)head; (void)jaw;
}

bool Rig::restore(std::vector<Joint> joints, std::vector<Mat4> inverseBind, std::vector<SkinWeights> weights) {
    if (joints.empty() || inverseBind.size() != joints.size()) return false;
    for (size_t i = 0; i < joints.size(); ++i) {
        /* Parents must precede children: computeWorld() walks the array once, in order. */
        if (joints[i].parent >= static_cast<int>(i)) return false;
    }
    joints_ = std::move(joints);
    inverseBind_ = std::move(inverseBind);
    weights_ = std::move(weights);
    shapeCache_.clear();
    field_ = nullptr;
    radiusScale_ = 1.0f;
    /* Rest-pose world positions, for the skeleton overlay and for joint introspection. */
    std::vector<Quat> rest(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) rest[i] = joints_[i].restRotation;
    const std::vector<Mat4> world = computeWorld(rest);
    for (size_t i = 0; i < joints_.size(); ++i) joints_[i].worldPosition = world[i].transformPoint(Vec3(0, 0, 0));
    return true;
}

std::vector<Mat4> Rig::computeWorld(const std::vector<Quat>& localRotations,
                                    const std::vector<Vec3>& localOffsets) const {
    const int n = jointCount();
    std::vector<Mat4> world(static_cast<size_t>(n), Mat4::identity());
    for (int i = 0; i < n; ++i) {
        const Joint& j = joints_[i];
        const Quat rot = (i < static_cast<int>(localRotations.size())) ? localRotations[i] : j.restRotation;
        const Vec3 off = (i < static_cast<int>(localOffsets.size())) ? localOffsets[i] : Vec3(0, 0, 0);
        const Mat4 local = Mat4::trs(j.restTranslation + off, rot, j.restScale);
        world[i] = (j.parent >= 0) ? (world[j.parent] * local) : local;
    }
    return world;
}

std::vector<Mat4> Rig::computeWorld(const std::vector<Quat>& localRotations) const {
    const int n = jointCount();
    std::vector<Mat4> world(static_cast<size_t>(n), Mat4::identity());
    for (int i = 0; i < n; ++i) {
        const Joint& j = joints_[i];
        const Quat rot = (i < static_cast<int>(localRotations.size())) ? localRotations[i] : j.restRotation;
        const Mat4 local = Mat4::trs(j.restTranslation, rot, j.restScale);
        world[i] = (j.parent >= 0) ? (world[j.parent] * local) : local;
    }
    return world;
}

/* ==============================================================================================
 * Skinning
 * ============================================================================================== */
namespace {

/** Distance from a point to a bone segment (its parent joint -> its own position). */
float pointSegmentDistance(const Vec3& p, const Vec3& a, const Vec3& b) {
    const Vec3 ab = b - a;
    const float denom = ab.lengthSq();
    if (denom < 1e-10f) return (p - a).length();
    const float t = clampf((p - a).dot(ab) / denom, 0.0f, 1.0f);
    return (p - (a + ab * t)).length();
}

/**
 * Region gate: a vertex may only bind to bones that plausibly drive it. Without this, a finger
 * bone can pick up chest vertices on a T-posed heavy build and the whole shoulder tears.
 */
bool boneAppliesToRegion(const Joint& j, int vertexRegion) {
    const bool isFinger = (j.flags & AF_JOINT_FINGER) != 0;
    const bool isHand = j.name.rfind("hand", 0) == 0;
    const bool isLeg = j.name.rfind("thigh", 0) == 0 || j.name.rfind("shin", 0) == 0;
    const bool isFoot = j.name.rfind("foot", 0) == 0 || j.name.rfind("toe", 0) == 0;
    const bool isFace = (j.flags & AF_JOINT_FACE) != 0;
    const bool isHeadBone = j.name == "head" || j.name == "neck_01";

    switch (vertexRegion) {
        case REGION_EYE_L: case REGION_EYE_R: case REGION_TEETH: case REGION_LIPS:
        case REGION_TONGUE:
            return isFace || isHeadBone || j.name == "jaw";
        case REGION_HAIR: case REGION_LASH:
            return isHeadBone || isFace;
        case REGION_NAIL:
            return isFinger || isFoot || isHand;
        case REGION_SHOE:
            return isFoot || isLeg;
        case REGION_GEAR:
            return true;                 /* armour is rigid across several bones by design */
        case REGION_CLOTH_UPPER:
        case REGION_CLOTH_LOWER:
            return !isFinger && !isFoot; /* cloth follows body bones, never fingers */
        default:
            /* Skin may bind to any bone. It used to be refused the finger bones here, to keep a
               finger from picking up chest skin; that also kept the fingers' own skin off them.
               The neighbourhood rule in computeWeights() is what keeps limbs apart now. */
            return true;
    }
}

} // namespace

void Rig::computeWeights(const BodyField& field, const Mesh& mesh) {
    const int n = jointCount();
    const size_t vcount = mesh.vertexCount();
    weights_.assign(vcount, SkinWeights{});
    const BodyMetrics& m = field.metrics();

    /*
     * Bones. A joint moves the part of the body between itself and its children: the thigh joint
     * sits at the hip and turns the thigh, the shin joint sits at the knee and turns the shin.
     * So the bone a vertex is measured against runs from a joint DOWN the chain to each child.
     *
     * It used to run the other way, from the parent to the joint. That gave the skin of every
     * limb segment to the joint at its far end - the thigh to the knee joint, the shin to the
     * ankle - and gave the root-to-hips "bone", a line from the floor to the pelvis between the
     * legs, a share of everything near it. Measured on a full-quality avatar: mid-shin skin was
     * 52 % foot and 44 % hips, and the fingertips were bound to the leg next to which they hang
     * (shin 39 %, hips 30 %, thigh 16 %, arm 0 %). Every pose away from rest tore the mesh.
     *
     * A capsule per bone, with the limb's radius at each end. A joint without children gets a
     * short bone of its own so that the tip it carries has something to be near.
     */
    struct Bone { int joint; Vec3 a, b; float ra, rb; };
    std::vector<Bone> bones;
    std::vector<std::vector<int>> children(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (joints_[static_cast<size_t>(i)].parent >= 0) children[static_cast<size_t>(joints_[static_cast<size_t>(i)].parent)].push_back(i);
    }
    auto isTwist = [&](int i) { return (joints_[static_cast<size_t>(i)].flags & AF_JOINT_TWIST) != 0; };
    auto isFaceJoint = [&](int i) { return (joints_[static_cast<size_t>(i)].flags & AF_JOINT_FACE) != 0; };
    for (int i = 0; i < n; ++i) {
        const Joint& j = joints_[static_cast<size_t>(i)];
        if (j.parent < 0) continue;      /* the root carries the whole figure and no skin of its own */
        /* A twist joint is never turned by a clip, so skin on it would move exactly as skin on
           its parent does. It is left out rather than competing with the real bones. */
        if (isTwist(i)) continue;
        const Vec3 a = j.worldPosition;
        if (isFaceJoint(i)) {            /* jaw and eyes: a point each, for the regions gated to them */
            bones.push_back({i, a, a, j.radius, j.radius});
            continue;
        }
        int made = 0;
        for (int c : children[static_cast<size_t>(i)]) {
            if (isTwist(c) || isFaceJoint(c)) continue;
            const Joint& child = joints_[static_cast<size_t>(c)];
            bones.push_back({i, a, child.worldPosition, j.radius, child.radius});
            ++made;
        }
        if (made > 0) continue;
        /* The end of a chain. */
        const Vec3 parentPos = joints_[static_cast<size_t>(j.parent)].worldPosition;
        const Vec3 along = a - parentPos;
        if (j.name == "head") {
            /* From the head joint up into the skull. */
            const float top = std::max(a.y + 0.02f, m.y.crownY - m.head.rx * 0.7f);
            bones.push_back({i, a, Vec3(a.x, top, a.z), j.radius, j.radius});
        } else if (j.name.rfind("toe", 0) == 0) {
            /* The toes, forward of the ball of the foot along the ground. */
            Vec3 forward(along.x, 0.0f, along.z);
            forward = forward.normalizedOr(Vec3(0, 0, -1));
            bones.push_back({i, a, a + forward * (along.length() * 0.55f), j.radius, j.radius * 0.8f});
        } else {
            /* A finger tip: the last phalanx, about as long as the one before it. */
            bones.push_back({i, a, a + along * 0.9f, j.radius, j.radius * 0.85f});
        }
    }

    /* How many joints apart two joints are in the skeleton tree. */
    std::vector<int> hops(static_cast<size_t>(n) * static_cast<size_t>(n), 1 << 20);
    for (int i = 0; i < n; ++i) {
        std::vector<int> dist(static_cast<size_t>(n), 1 << 20);
        std::vector<int> queue{i};
        dist[static_cast<size_t>(i)] = 0;
        for (size_t q = 0; q < queue.size(); ++q) {
            const int at = queue[q];
            auto visit = [&](int next) {
                if (next < 0 || dist[static_cast<size_t>(next)] <= dist[static_cast<size_t>(at)] + 1) return;
                dist[static_cast<size_t>(next)] = dist[static_cast<size_t>(at)] + 1;
                queue.push_back(next);
            };
            visit(joints_[static_cast<size_t>(at)].parent);
            for (int c : children[static_cast<size_t>(at)]) visit(c);
        }
        for (int k = 0; k < n; ++k) hops[static_cast<size_t>(i) * static_cast<size_t>(n) + static_cast<size_t>(k)] = dist[static_cast<size_t>(k)];
    }

    /*
     * A vertex belongs first of all to the bone it lies on: the one whose capsule it is nearest,
     * measured in radii. It may share its weight only with bones within three joints of that one
     * along the skeleton. Distance in space alone is not enough - a hand hangs a few centimetres
     * from a thigh that is ten joints away from it - and the neighbourhood is what stops one limb
     * taking hold of another. Three joints is what a shoulder needs: the ribs under the armpit
     * (spine) reach the upper arm through chest and clavicle.
     */
    const int kMaxHops = 3;
    const float falloff = 4.5f;   /* higher = tighter: the blend at a joint spans about one limb radius */
    const int threads = defaultThreadCount();
    parallelFor(static_cast<int>(vcount), threads, [&](int i0, int i1) {
        std::vector<float> nearness(static_cast<size_t>(n));
        std::vector<std::pair<float, int>> scored;
        for (int vi = i0; vi < i1; ++vi) {
            const Vec3 p = mesh.positions[static_cast<size_t>(vi)];
            const int region = (vi < static_cast<int>(mesh.regions.size())) ? mesh.regions[static_cast<size_t>(vi)] : REGION_SKIN;
            /* Per joint, the smallest distance in radii over that joint's bones. */
            std::fill(nearness.begin(), nearness.end(), 1e30f);
            int primary = -1;
            float primaryU = 1e30f;
            for (const Bone& bone : bones) {
                const Joint& j = joints_[static_cast<size_t>(bone.joint)];
                if (!boneAppliesToRegion(j, region)) continue;
                const Vec3 ab = bone.b - bone.a;
                const float denom = ab.lengthSq();
                const float t = denom < 1e-10f ? 0.0f : clampf((p - bone.a).dot(ab) / denom, 0.0f, 1.0f);
                const float d = (p - (bone.a + ab * t)).length();
                const float r = maxf(bone.ra + (bone.rb - bone.ra) * t, 1e-3f);
                const float u = d / r;
                if (u < nearness[static_cast<size_t>(bone.joint)]) nearness[static_cast<size_t>(bone.joint)] = u;
                if (u < primaryU) { primaryU = u; primary = bone.joint; }
            }
            if (primary < 0) {
                /* No bone is allowed for this region: the nearest one of all, as before. */
                float best = 1e30f;
                int bestJoint = 0;
                for (const Bone& bone : bones) {
                    const float d = pointSegmentDistance(p, bone.a, bone.b);
                    if (d < best) { best = d; bestJoint = bone.joint; }
                }
                weights_[static_cast<size_t>(vi)].joints = {static_cast<uint16_t>(bestJoint), 0, 0, 0};
                weights_[static_cast<size_t>(vi)].weights = {1.0f, 0.0f, 0.0f, 0.0f};
                continue;
            }
            scored.clear();
            const int* fromPrimary = &hops[static_cast<size_t>(primary) * static_cast<size_t>(n)];
            for (int jIndex = 0; jIndex < n; ++jIndex) {
                const float u = nearness[static_cast<size_t>(jIndex)];
                if (u >= 1e29f || fromPrimary[jIndex] > kMaxHops) continue;
                /* Floored so that a vertex on a bone's axis does not divide by zero. */
                scored.emplace_back(1.0f / std::pow(u + 0.05f, falloff), jIndex);
            }
            const size_t keep = std::min<size_t>(4, scored.size());
            std::partial_sort(scored.begin(), scored.begin() + static_cast<long>(keep), scored.end(),
                              [](const std::pair<float, int>& x, const std::pair<float, int>& y) {
                                  return x.first > y.first;
                              });
            SkinWeights sw;
            float sum = 0.0f;
            for (size_t k = 0; k < keep; ++k) {
                sw.joints[k] = static_cast<uint16_t>(scored[k].second);
                sw.weights[k] = scored[k].first;
                sum += sw.weights[k];
            }
            if (sum > 1e-9f) {
                /* A share below one percent moves nothing visible; give it to the others. */
                float kept = 0.0f;
                for (size_t k = 0; k < keep; ++k) {
                    if (sw.weights[k] / sum < 0.01f) { sw.weights[k] = 0.0f; sw.joints[k] = sw.joints[0]; }
                    kept += sw.weights[k];
                }
                for (size_t k = 0; k < keep; ++k) sw.weights[k] /= kept;
            } else {
                sw.joints = {static_cast<uint16_t>(primary), 0, 0, 0};
                sw.weights = {1.0f, 0.0f, 0.0f, 0.0f};
            }
            weights_[static_cast<size_t>(vi)] = sw;
        }
    });
}

void Rig::skin(const Mesh& restMesh, const std::vector<Mat4>& jointWorld, std::vector<Vec3>* outPositions) const {
    const size_t vcount = restMesh.vertexCount();
    outPositions->resize(vcount);
    const int threads = defaultThreadCount();
    parallelFor(static_cast<int>(vcount), threads, [&](int i0, int i1) {
        for (int vi = i0; vi < i1; ++vi) {
            const SkinWeights& sw = weights_[static_cast<size_t>(vi)];
            const Vec3 p = restMesh.positions[static_cast<size_t>(vi)];
            Vec3 acc(0, 0, 0);
            float total = 0.0f;
            for (int k = 0; k < 4; ++k) {
                const float w = sw.weights[k];
                if (w <= 0.0f) continue;
                const size_t ji = sw.joints[k];
                if (ji >= jointWorld.size()) continue;
                const Mat4& bind = inverseBind_[ji];
                const Mat4 m = jointWorld[ji] * bind;      /* skinning matrix */
                acc += m.transformPoint(p) * w;
                total += w;
            }
            (*outPositions)[static_cast<size_t>(vi)] = (total > 1e-6f) ? (acc / total) : p;
        }
    });
}

/* ==============================================================================================
 * Facial blendshapes
 * ============================================================================================== */
namespace {

/*
 * Projects a nominal facial anchor onto the skin: the FIRST surface crossing in front of the face.
 *
 * The facial anchors used to be pinned to a nominal "face plane" a fixed fraction of the skull's
 * front radius, which is only correct for the eyeballs. The brow ridge, the nose bridge, the lips
 * and the cheeks all sit 8-25 mm in FRONT of that plane, and how far depends on the build, so a
 * stamp centred there still touched the surface - but only through the far tail of its falloff.
 * Measured effect: every brow channel produced deltas of a few microns, i.e. no visible motion.
 *
 * A 3D Newton march along the gradient (the first fix) put the brow, cheek and nose anchors on the
 * skin, but it is not safe at the lips: the mouth groove is a narrow channel between two surfaces,
 * and a march that starts outside the face can pass the lip skin and settle on the mucosa INSIDE
 * the mouth. Whether it does depends on how the field is evaluated - with the lattice cache the
 * engine builds, the mouth anchor landed 14 mm too deep (measured: mouth level z = -0.1185 in the
 * engine vs -0.1046 without the cache), which collapsed the eight channels stamped there
 * (mouthUpperUp/Press/Dimple/Stretch) to a few hundredths of a millimetre. An empty morph target
 * is then dropped from the .glb, which renumbers every channel after it.
 *
 * So the anchor is found by scanning inward along -z: the first sign change is the surface that
 * faces the viewer, independent of field representation, and 60 evaluations per anchor at build
 * time is nothing. Bisection then refines it to well under a tenth of a millimetre.
 */
Vec3 surfaceAnchor(const BodyField& field, float x, float y, float startZ = -0.16f) {
    const float step = 0.0015f;          /* 1.5 mm scan */
    const int maxSteps = 90;             /* 135 mm of travel, well past any face */
    float zPrev = startZ;
    float dPrev = bodyUnionField(field, Vec3(x, y, zPrev));
    if (dPrev <= 0.0f) return Vec3(x, y, zPrev);          /* already inside: nothing to do */
    for (int i = 1; i <= maxSteps; ++i) {
        const float z = startZ + step * static_cast<float>(i);
        const float d = bodyUnionField(field, Vec3(x, y, z));
        if (d <= 0.0f) {
            /* Bisect the bracketed interval [zPrev, z] - 20 rounds is 1.5 mm / 10^6. */
            float lo = zPrev, hi = z;
            for (int k = 0; k < 20; ++k) {
                const float mid = 0.5f * (lo + hi);
                if (bodyUnionField(field, Vec3(x, y, mid)) > 0.0f) lo = mid; else hi = mid;
            }
            return Vec3(x, y, 0.5f * (lo + hi));
        }
        zPrev = z;
        dPrev = d;
    }
    /* No surface within the scan (a feature that does not exist on this build): stay on the last
       outside sample rather than returning a point inside the head. */
    (void)dPrev;
    return Vec3(x, y, zPrev);
}
} // namespace

void Rig::buildBlendshapes(const BodyField& field, const Mesh& mesh) {
    shapeCache_.assign(kBlendshapeCount, {});
    const BodyMetrics& m = field.metrics();
    const float headH = m.y.crownY - m.y.chinY;
    const float faceZ = -m.head.rz * 0.98f;
    const Vec3 eyeL(m.eyeSeparation * 0.5f, m.y.eyeY, faceZ * 0.80f);
    const Vec3 eyeR(-m.eyeSeparation * 0.5f, m.y.eyeY, faceZ * 0.80f);
    const Vec3 mouth(0.0f, m.mouthY, faceZ * 0.94f);
    const float lip = m.lipWidth;

    /*
     * Skin anchors - see surfaceAnchor(). One per feature, on the surface itself.
     *
     * The anchors are solved on the +x side and mirrored, rather than solved for both sides
     * independently: the field is symmetric about x = 0 by construction, but a gradient march is
     * not, and solving each side separately let the right-hand march settle on the cornea while
     * its mirror settled on the lid - which is exactly how a channel ends up moving nothing on one
     * side of the face and something on the other.
     *
     * The lids get analytic anchors instead of a march, taken straight from the ellipsoids the
     * field itself builds for them (see BodyField::browShape): the upper lid sits at
     * eye + (0, +0.62r, -0.22r) with a z radius of 0.95r, so its front surface is at
     * eye + (0, +0.62r, -1.17r). That is where the skin actually is when the eye is open, and it
     * is why the blink and wide channels finally move the lid rather than the sclera.
     */
    const float rEye = m.eyeRadius;
    const Vec3 eyeSkinL = surfaceAnchor(field, m.eyeSeparation * 0.5f, m.y.eyeY);
    const Vec3 eyeSkinR(-eyeSkinL.x, eyeSkinL.y, eyeSkinL.z);
    const Vec3 browSkinL = surfaceAnchor(field, m.eyeSeparation * 0.5f, m.y.eyeY + rEye * 1.9f);
    const Vec3 browSkinR(-browSkinL.x, browSkinL.y, browSkinL.z);
    const Vec3 browSkinC = surfaceAnchor(field, 0.0f, eyeL.y + rEye * 2.25f);
    const Vec3 eyeTrueL = field.eyeCentre(0);
    const Vec3 eyeTrueR = field.eyeCentre(1);
    const Vec3 lidUpperL = eyeTrueL + Vec3(0.0f, rEye * 0.62f, -rEye * 1.17f);
    const Vec3 lidUpperR = eyeTrueR + Vec3(0.0f, rEye * 0.62f, -rEye * 1.17f);
    const Vec3 lidLowerL = eyeTrueL + Vec3(0.0f, -rEye * 0.66f, -rEye * 1.12f);
    const Vec3 lidLowerR = eyeTrueR + Vec3(0.0f, -rEye * 0.66f, -rEye * 1.12f);
    /* The lips protrude: stamp on the vermilion, not on the mouth's nominal plane. */
    const float mouthSkinZ = surfaceAnchor(field, 0.0f, m.mouthY).z;
    const Vec3 cheekSkinL = surfaceAnchor(field, m.faceWidth * 0.40f, m.mouthY + headH * 0.02f);
    const Vec3 cheekSkinR = surfaceAnchor(field, -m.faceWidth * 0.40f, m.mouthY + headH * 0.02f);
    const Vec3 noseSkinL = surfaceAnchor(field, m.head.rx * 0.28f, m.mouthY + headH * 0.05f);
    const Vec3 noseSkinR = surfaceAnchor(field, -m.head.rx * 0.28f, m.mouthY + headH * 0.05f);

    /*
     * The lip line: where the lips meet, found on the field itself. The nominal mouth height is
     * only where the lips were modelled around. The line a jaw has to cut along is the groove
     * between them: each lip is a ridge (the surface comes furthest forward there), and the groove
     * is the deepest point between the two ridges. Looking for the deepest point near the mouth
     * without the ridges finds the hollow under the lower lip instead, and the jaw then drags a
     * curtain of skin down from closed lips. It is searched in the middle and beside it, because
     * the line usually curves.
     */
    auto grooveY = [&](float x) {
        const float step = 0.00025f;
        const int half = 44;                                /* 11 mm either side of the mouth */
        std::vector<float> z(static_cast<size_t>(2 * half + 1));
        for (int i = -half; i <= half; ++i) {
            z[static_cast<size_t>(i + half)] = surfaceAnchor(field, x, m.mouthY + static_cast<float>(i) * step).z;
        }
        int lower = 0, upper = 2 * half;                    /* the ridges: most forward = smallest z */
        for (int i = 0; i <= half; ++i) if (z[static_cast<size_t>(i)] < z[static_cast<size_t>(lower)]) lower = i;
        for (int i = half; i <= 2 * half; ++i) if (z[static_cast<size_t>(i)] < z[static_cast<size_t>(upper)]) upper = i;
        int groove = half;
        for (int i = lower; i <= upper; ++i) if (z[static_cast<size_t>(i)] > z[static_cast<size_t>(groove)]) groove = i;
        return m.mouthY + static_cast<float>(groove - half) * step;
    };
    const float lipLineY = grooveY(0.0f);
    const float lipLineSideX = lip * 0.32f;
    const float lipLineSideY = grooveY(lipLineSideX);
    const float lipCurve = clampf((lipLineSideY - lipLineY) / (lipLineSideX * lipLineSideX), -12.0f, 12.0f);

    FaceRig face;
    face.boxMin = Vec3(-m.head.rx * 1.6f, m.y.chinY - headH * 0.10f, -10.0f);
    face.boxMax = Vec3(m.head.rx * 1.6f, m.y.crownY + headH * 0.05f, 0.0f);
    auto channelOf = [](const char* name) {
        for (int c = 0; c < kBlendshapeCount; ++c) if (std::strcmp(kBlendshapeNames[c], name) == 0) return c;
        return -1;
    };
    /* One stamp: offset = falloff(p; centre, radii) * (dir + (p - centre) * scale). */
    auto stampK = [&](const char* name, const Vec3& centre, const Vec3& radii, const Vec3& dir,
                      const Vec3& scale = Vec3(0, 0, 0), int flags = 0) {
        FaceKernel k;
        k.channel = channelOf(name);
        k.centre = centre; k.radii = radii; k.dir = dir; k.scale = scale; k.flags = flags;
        if (k.channel >= 0) face.kernels.push_back(k);
    };
    /* The lower jaw as one piece, see FaceRig::kernelWeight. */
    auto jawK = [&](const char* name, const Vec3& dir) {
        FaceKernel k;
        k.channel = channelOf(name);
        k.centre = Vec3(0.0f, m.y.chinY, mouthSkinZ);
        k.radii = Vec3(m.jaw.rx * 1.7f, headH * 0.09f, m.head.rz * 1.05f);
        k.dir = dir;
        k.scale = Vec3(lip * 0.50f, (lipLineY - m.y.chinY) * 0.60f, lipCurve);
        k.flags = AF_FACE_KERNEL_JAW;
        k.cutY = lipLineY;
        if (k.channel >= 0) face.kernels.push_back(k);
    };

    /*
     * Jaw. These four channels used to be stamped about a point in the MIDDLE of the head (z = 0),
     * so the falloff had all but run out by the time it reached the skin: jawOpen moved the chin
     * by a few millimetres and the lips not at all. They are now the jaw kernel: everything below
     * the lip line moves, so jawOpen parts the lips.
     */
    jawK("jawOpen", Vec3(0.0f, -headH * 0.105f, headH * 0.020f));
    jawK("jawForward", Vec3(0.0f, 0.0f, -headH * 0.035f));
    jawK("jawLeft", Vec3(headH * 0.05f, 0.0f, 0.0f));
    jawK("jawRight", Vec3(-headH * 0.05f, 0.0f, 0.0f));

    for (int side = 0; side < 2; ++side) {
        const bool left = side == 0;
        const float sx = left ? 1.0f : -1.0f;
        auto nm = [&](const char* base) { return std::string(base) + (left ? "Left" : "Right"); };
        const Vec3 corner(sx * lip * 0.5f, mouth.y, mouthSkinZ);
        stampK(nm("mouthSmile").c_str(), corner, Vec3(lip * 0.75f, lip * 0.55f, lip * 0.8f), Vec3(sx * lip * 0.30f, lip * 0.22f, 0.0f));
        stampK(nm("mouthFrown").c_str(), corner, Vec3(lip * 0.75f, lip * 0.55f, lip * 0.8f), Vec3(0.0f, -lip * 0.24f, 0.0f));
        stampK(nm("mouthPress").c_str(), Vec3(sx * lip * 0.4f, mouth.y, mouthSkinZ), Vec3(lip * 0.5f, lip * 0.4f, lip * 0.7f), Vec3(0.0f, -lip * 0.10f, 0.0f));
        stampK(nm("mouthUpperUp").c_str(), Vec3(sx * lip * 0.4f, mouth.y + lip * 0.20f, mouthSkinZ), Vec3(lip * 0.55f, lip * 0.40f, lip * 0.7f), Vec3(0.0f, lip * 0.20f, 0.0f));
        stampK(nm("mouthLowerDown").c_str(), Vec3(sx * lip * 0.4f, mouth.y - lip * 0.20f, mouthSkinZ), Vec3(lip * 0.55f, lip * 0.40f, lip * 0.7f), Vec3(0.0f, -lip * 0.20f, 0.0f));
        stampK(left ? "mouthLeft" : "mouthRight", mouth, Vec3(lip * 0.9f, lip * 0.5f, lip * 0.8f), Vec3(sx * lip * 0.28f, 0.0f, 0.0f));
        stampK(nm("mouthDimple").c_str(), Vec3(sx * lip * 0.62f, mouth.y, mouthSkinZ), Vec3(lip * 0.4f, lip * 0.4f, lip * 0.6f), Vec3(sx * lip * 0.16f, 0.0f, lip * 0.10f));
        stampK(nm("mouthStretch").c_str(), Vec3(sx * lip * 0.6f, mouth.y, mouthSkinZ), Vec3(lip * 0.6f, lip * 0.4f, lip * 0.7f), Vec3(sx * lip * 0.30f, 0.0f, 0.0f));

        /* The upper lid rotates down over the globe: the lid surface moves furthest, the canthi
           least, so the displacement is stamped with a wide, flat mask spanning the whole lid. */
        const Vec3 lidU = left ? lidUpperL : lidUpperR;
        const Vec3 lidD = left ? lidLowerL : lidLowerR;
        stampK(nm("eyeBlink").c_str(), lidU, Vec3(rEye * 2.4f, rEye * 1.5f, rEye * 2.6f), Vec3(0.0f, -rEye * 1.55f, rEye * 0.30f));
        stampK(nm("eyeWide").c_str(), lidU, Vec3(rEye * 2.4f, rEye * 1.6f, rEye * 2.6f), Vec3(0.0f, rEye * 0.95f, 0.0f));
        stampK(nm("eyeWide").c_str(), lidD, Vec3(rEye * 2.2f, rEye * 1.4f, rEye * 2.4f), Vec3(0.0f, -rEye * 0.45f, 0.0f));
        /* Squint = lower lid up + cheek raised towards the outer canthus. */
        stampK(nm("eyeSquint").c_str(), lidD, Vec3(rEye * 2.6f, rEye * 1.6f, rEye * 2.6f), Vec3(0.0f, rEye * 0.75f, -rEye * 0.20f));
        /*
         * Gaze channels only move the eyeball islands; the lids react through the corresponding
         * eyeBlink/eyeWide channels, exactly as ARKit does. The mask peaks on the globe's surface
         * (it falls off over 1.6 radii, so the skin at 1.6r gets nothing and the sclera at 1.0r
         * most of it). 0.9 radius of movement at the surface is about 7 degrees of gaze for a
         * 12 mm eye, the range ARKit's eyeLook channels are authored for.
         */
        const Vec3 eye = left ? eyeL : eyeR;
        const Vec3 globe(m.eyeRadius * 1.6f, m.eyeRadius * 1.6f, m.eyeRadius * 1.6f);
        stampK(nm("eyeLookUp").c_str(), eye, globe, Vec3(0.0f, m.eyeRadius * 0.90f, 0.0f));
        stampK(nm("eyeLookDown").c_str(), eye, globe, Vec3(0.0f, -m.eyeRadius * 0.90f, 0.0f));
        stampK(nm("eyeLookOut").c_str(), eye, globe, Vec3(sx * m.eyeRadius * 0.90f, 0.0f, 0.0f));
        stampK(nm("eyeLookIn").c_str(), eye, globe, Vec3(-sx * m.eyeRadius * 0.90f, 0.0f, 0.0f));

        const Vec3 brow = left ? browSkinL : browSkinR;
        stampK(nm("browDown").c_str(), brow, Vec3(m.eyeRadius * 2.2f, m.eyeRadius * 1.1f, m.eyeRadius * 1.6f), Vec3(0.0f, -headH * 0.016f, -headH * 0.006f));
        stampK(nm("browOuterUp").c_str(), brow, Vec3(m.eyeRadius * 1.6f, m.eyeRadius * 1.2f, m.eyeRadius * 1.6f), Vec3(0.0f, headH * 0.020f, 0.0f));
        const Vec3 cheek = left ? cheekSkinL : cheekSkinR;
        stampK(nm("cheekSquint").c_str(), Vec3(cheek.x, eye.y - m.eyeRadius * 1.5f, cheek.z),
               Vec3(m.eyeRadius * 2.4f, m.eyeRadius * 1.8f, m.eyeRadius * 2.0f), Vec3(0.0f, headH * 0.014f, -headH * 0.004f));
        const Vec3 nose = left ? noseSkinL : noseSkinR;
        stampK(nm("noseSneer").c_str(), nose, Vec3(m.head.rx * 0.36f, headH * 0.055f, m.head.rz * 0.55f), Vec3(sx * headH * 0.008f, headH * 0.012f, 0.0f));
    }
    stampK("mouthPucker", mouth, Vec3(lip * 0.85f, lip * 0.55f, lip * 0.85f), Vec3(0.0f, 0.0f, -lip * 0.34f), Vec3(0.0f, 0.10f, 0.0f));
    stampK("mouthFunnel", mouth, Vec3(lip * 0.85f, lip * 0.55f, lip * 0.85f), Vec3(0.0f, 0.0f, -lip * 0.20f), Vec3(0.0f, 0.10f, 0.0f));
    stampK("mouthRollUpper", Vec3(0, mouth.y + lip * 0.18f, mouthSkinZ), Vec3(lip * 0.55f, lip * 0.30f, lip * 0.7f), Vec3(0.0f, lip * 0.12f, lip * 0.08f));
    stampK("mouthRollLower", Vec3(0, mouth.y - lip * 0.18f, mouthSkinZ), Vec3(lip * 0.55f, lip * 0.30f, lip * 0.7f), Vec3(0.0f, -lip * 0.12f, lip * 0.08f));
    stampK("mouthShrugUpper", Vec3(0, mouth.y + lip * 0.3f, mouthSkinZ), Vec3(lip * 0.8f, lip * 0.5f, lip * 0.8f), Vec3(0.0f, lip * 0.16f, 0.0f));
    stampK("mouthShrugLower", Vec3(0, mouth.y - lip * 0.3f, mouthSkinZ), Vec3(lip * 0.8f, lip * 0.5f, lip * 0.8f), Vec3(0.0f, -lip * 0.16f, 0.0f));
    stampK("mouthClose", Vec3(0, mouth.y, mouthSkinZ), Vec3(lip * 0.7f, lip * 0.6f, lip * 0.8f), Vec3(0.0f, -lip * 0.10f, 0.0f));
    /* Stamped on the glabella (between the brows) rather than at the eyeball plane: the forehead
       sits further forward than the eye centres, so the mask is anchored to the skull radius to
       guarantee that it covers brow vertices. */
    stampK("browInnerUp", browSkinC, Vec3(m.eyeRadius * 2.6f, headH * 0.045f, m.head.rz * 0.55f), Vec3(0.0f, headH * 0.022f, -headH * 0.004f));
    stampK("cheekPuff", Vec3(0, m.mouthY, mouthSkinZ), Vec3(m.faceWidth * 0.62f, headH * 0.20f, m.head.rz * 0.95f),
           Vec3(m.faceWidth * 0.10f, 0.0f, -headH * 0.012f), Vec3(0, 0, 0), AF_FACE_KERNEL_MIRROR_X);
    /* The tongue is a separate island; the channel moves it forward and down. The region is a
       box: in front of z = -0.4 skull radii, from 0.09 head heights below the mouth to 0.04 above. */
    stampK("tongueOut", Vec3(0.0f, m.mouthY - headH * 0.025f, -m.head.rz * 0.4f - 5.0f), Vec3(10.0f, headH * 0.065f, 5.0f),
           Vec3(0.0f, -headH * 0.020f, -headH * 0.055f), Vec3(0, 0, 0), AF_FACE_KERNEL_BOX);
    std::stable_sort(face.kernels.begin(), face.kernels.end(),
                     [](const FaceKernel& a, const FaceKernel& b) { return a.channel < b.channel; });
    faceRig_ = face;

    /* The morph targets a GLB carries: every channel's offset at every face vertex. */
    for (int c = 0; c < kBlendshapeCount; ++c) shapeCache_[static_cast<size_t>(c)].assign(mesh.vertexCount(), Vec3(0, 0, 0));
    for (size_t vi = 0; vi < mesh.vertexCount(); ++vi) {
        const Vec3 p = mesh.positions[vi];
        if (!faceRig_.contains(p)) continue;
        for (const FaceKernel& k : faceRig_.kernels) {
            shapeCache_[static_cast<size_t>(k.channel)][vi] += FaceRig::kernelDelta(k, p);
        }
    }
}

const std::vector<Vec3>& Rig::blendshapeDeltas(int channel) const {
    if (shapeCache_.empty()) {
        static const std::vector<Vec3> kEmpty;
        return kEmpty;
    }
    const int c = (channel < 0) ? 0 : ((channel >= kBlendshapeCount) ? kBlendshapeCount - 1 : channel);
    return shapeCache_[static_cast<size_t>(c)];
}

void Rig::deform(const Mesh& restMesh, const std::vector<Mat4>& jointWorld,
                 const std::vector<float>& blendshapeWeights, std::vector<Vec3>* outPositions) const {
    skin(restMesh, jointWorld, outPositions);
    if (blendshapeWeights.empty() || shapeCache_.empty()) return;
    const size_t vcount = outPositions->size();
    for (int c = 0; c < kBlendshapeCount && c < static_cast<int>(blendshapeWeights.size()); ++c) {
        const float w = blendshapeWeights[static_cast<size_t>(c)];
        if (std::fabs(w) < 1e-5f) continue;
        const std::vector<Vec3>& deltas = shapeCache_[static_cast<size_t>(c)];
        if (deltas.size() != vcount) continue;
        for (size_t i = 0; i < vcount; ++i) (*outPositions)[i] += deltas[i] * w;
    }
}

/* ==============================================================================================
 * Animation clips
 * ============================================================================================== */
Vec3 Track::sampleTranslation(float t) const {
    if (keys.empty()) return Vec3(0, 0, 0);
    if (keys.size() == 1) return keys[0].translation;
    if (t <= keys.front().time) return keys.front().translation;
    if (t >= keys.back().time) return keys.back().translation;
    size_t hi = 1;
    while (hi < keys.size() - 1 && keys[hi].time < t) ++hi;
    const Keyframe& a = keys[hi - 1];
    const Keyframe& b = keys[hi];
    const float span = b.time - a.time;
    const float u = span < 1e-6f ? 0.0f : (t - a.time) / span;
    const float s = u * u * (3.0f - 2.0f * u);
    return a.translation.lerp(b.translation, s);
}

Quat Track::sample(float t) const {
    if (keys.empty()) return Quat::identity();
    if (keys.size() == 1) return keys[0].rotation;
    if (t <= keys.front().time) return keys.front().rotation;
    if (t >= keys.back().time) return keys.back().rotation;
    size_t hi = 1;
    while (hi < keys.size() - 1 && keys[hi].time < t) ++hi;
    const Keyframe& a = keys[hi - 1];
    const Keyframe& b = keys[hi];
    const float span = b.time - a.time;
    const float u = span < 1e-6f ? 0.0f : (t - a.time) / span;
    /* Smoothstep easing between authored keys: avoids the linear-slerp "robotic" snap that is
       the usual tell of an unstyled procedural animation. */
    const float s = u * u * (3.0f - 2.0f * u);
    return Quat::slerp(a.rotation, b.rotation, s);
}

namespace {

/** Helper: quaternion from degrees around X/Y/Z in that order. */
Quat rot(float xDeg, float yDeg, float zDeg) {
    return Quat::fromEulerXYZ(Vec3(radians(xDeg), radians(yDeg), radians(zDeg)));
}

/*
 * The clip library.
 *
 * Every clip is authored key data: Euler angles in degrees on named joints, turned into
 * quaternions here and interpolated with an ease between keys. The rig they are written for:
 *
 *   - faces -Z, +Y is up, the joints named _L are on +X (the figure's own right hand side);
 *   - every joint's rest rotation is the identity, the arms hang about 14 degrees out;
 *   - rot(x, y, z) turns about X first, then Y, then Z.
 *
 * What a positive angle does, which is the whole grammar of this file:
 *
 *   X on a limb that hangs down (thigh, upper arm)   swings it FORWARD
 *   X on the shin                                    negative bends the knee (heel back)
 *   X on the forearm                                 positive bends the elbow (hand forward)
 *   X on the foot                                    toes up
 *   X on hips, spine, neck, head, root               leans BACK; negative leans forward, nods down
 *   Y                                                turns the +X side forward
 *   Z on an _L limb                                  lifts it out to the side; _R needs negative
 *
 * Until this revision half of the library had been written for the opposite sign of X (walk,
 * jog, run, punch and the forearms everywhere), so knees and elbows bent the wrong way; t_pose and
 * a_pose turned the arms about the vertical, which twists a hanging arm instead of raising it;
 * squat and sit kept the hips at standing height, so the feet left the floor instead. Nobody had
 * looked: the skin weights were wrong too (see computeWeights) and hid it. Each clip below was
 * drawn from the front and the side before it was kept.
 *
 * Root translations are written for a leg (hip joint to ankle) of 0.86 m and are scaled to the
 * figure's own leg when the clips are sampled (Rig::motionScale), so a squat keeps the feet on
 * the floor for any height.
 */
struct Key { float t; float x, y, z; };

class ClipBuilder {
public:
    ClipBuilder(const char* name, float duration, bool looping) {
        clip_.name = name;
        clip_.duration = duration;
        clip_.looping = looping;
    }
    /** One joint. */
    ClipBuilder& joint(const std::string& name, std::initializer_list<Key> keys) {
        clip_.tracks.push_back(trackOf(name, keys));
        return *this;
    }
    /**
     * Both sides. The keys are those of the _L joint; the _R joint gets their mirror image (the
     * turns about Y and Z reversed), `phase` seconds later round the loop. A gait is one leg's
     * curve written once and the other leg half a cycle behind.
     */
    ClipBuilder& both(const std::string& base, std::initializer_list<Key> keys, float phase = 0.0f) {
        const Track left = trackOf(base + "_L", keys);
        Track right;
        right.jointName = base + "_R";
        const float duration = clip_.duration;
        for (const Keyframe& k : left.keys) {
            Keyframe m = k;
            m.rotation = Quat(k.rotation.x, -k.rotation.y, -k.rotation.z, k.rotation.w);
            right.keys.push_back(m);
        }
        if (phase > 1e-6f && duration > 1e-6f) {
            /* Move every key `phase` later, wrap the ones that pass the end, and close the loop
               with what the curve holds at the seam. */
            Track mirrored = right;
            std::vector<Keyframe> moved;
            for (const Keyframe& k : mirrored.keys) {
                Keyframe m = k;
                m.time = k.time + phase;
                if (m.time > duration + 1e-5f) m.time -= duration;
                if (m.time > 1e-4f && m.time < duration - 1e-4f) moved.push_back(m);
            }
            std::sort(moved.begin(), moved.end(), [](const Keyframe& x, const Keyframe& y) { return x.time < y.time; });
            Keyframe seam;
            seam.rotation = mirrored.sample(duration - phase);
            seam.time = 0.0f;
            moved.insert(moved.begin(), seam);
            seam.time = duration;
            moved.push_back(seam);
            right.keys = std::move(moved);
        }
        clip_.tracks.push_back(left);
        clip_.tracks.push_back(right);
        return *this;
    }
    /** The root: where the whole figure is, and how it is turned. Metres for a 0.86 m leg. */
    struct RootKey { float t; float rx, ry, rz; float x, y, z; };
    ClipBuilder& root(std::initializer_list<RootKey> keys) {
        Track tr;
        tr.jointName = "root";
        for (const RootKey& k : keys) {
            Keyframe kf;
            kf.time = k.t;
            kf.rotation = rot(k.rx, k.ry, k.rz);
            kf.translation = Vec3(k.x, k.y, k.z);
            tr.keys.push_back(kf);
        }
        clip_.tracks.push_back(std::move(tr));
        return *this;
    }
    Clip build() const { return clip_; }

private:
    static Track trackOf(const std::string& name, std::initializer_list<Key> keys) {
        Track tr;
        tr.jointName = name;
        for (const Key& k : keys) {
            Keyframe kf;
            kf.time = k.t;
            kf.rotation = rot(k.x, k.y, k.z);
            tr.keys.push_back(kf);
        }
        return tr;
    }
    Clip clip_;
};

const std::vector<Clip>& library() {
    static std::vector<Clip> clips = [] {
        std::vector<Clip> out;

        /* --- idle: weight shift and breathing, 4 s loop ----------------------------------- */
        out.push_back(ClipBuilder("idle", 4.0f, true)
            .joint("hips", {{0, 0, 0, 0}, {1, 0, 1.2f, 0.8f}, {2, 0, 0, 0}, {3, 0, -1.2f, -0.8f}, {4, 0, 0, 0}})
            .joint("spine_02", {{0, 1.0f, 0, 0}, {2, 2.4f, 0, 0}, {4, 1.0f, 0, 0}})
            .joint("chest", {{0, -1.6f, 0, 0}, {2, -3.0f, 0, 0}, {4, -1.6f, 0, 0}})
            .joint("head", {{0, 0, -3.0f, 0}, {2, 1.0f, 2.5f, 0.6f}, {4, 0, -3.0f, 0}})
            .both("upperarm_01", {{0, 0, 0, 2.0f}, {2, 1.5f, 0, 4.5f}, {4, 0, 0, 2.0f}})
            .both("forearm_01", {{0, 8.0f, 0, 0}, {2, 11.0f, 0, 0}, {4, 8.0f, 0, 0}})
            .build());

        /* --- breathe: respiration on its own, for looking at a face ----------------------- */
        out.push_back(ClipBuilder("breathe", 5.0f, true)
            .joint("chest", {{0, 0, 0, 0}, {2.5f, -4.5f, 0, 0}, {5, 0, 0, 0}})
            .joint("neck_01", {{0, 0, 0, 0}, {2.5f, 1.5f, 0, 0}, {5, 0, 0, 0}})
            .build());

        /* --- walk: 1.1 s cycle. Heel strike at 0, flat foot, mid stance, heel off, toe off,
               swing with the knee folded for clearance, reach. The pelvis drops at each heel
               strike because both legs are spread; that is the bob. ------------------------- */
        out.push_back(ClipBuilder("walk", 1.1f, true)
            .root({{0, 0, 0, 0, 0, -0.050f, 0}, {0.275f, 0, 0, 0, 0, -0.006f, 0}, {0.55f, 0, 0, 0, 0, -0.050f, 0},
                   {0.825f, 0, 0, 0, 0, -0.006f, 0}, {1.1f, 0, 0, 0, 0, -0.050f, 0}})
            .joint("hips", {{0, 0, 4.0f, 0}, {0.275f, 0, 0, 0}, {0.55f, 0, -4.0f, 0}, {0.825f, 0, 0, 0}, {1.1f, 0, 4.0f, 0}})
            .joint("spine_02", {{0, -2.0f, -5.0f, 0}, {0.55f, -2.0f, 5.0f, 0}, {1.1f, -2.0f, -5.0f, 0}})
            .both("thigh", {{0, 24, 0, 0}, {0.154f, 18, 0, 0}, {0.275f, 4, 0, 0}, {0.44f, -12, 0, 0}, {0.55f, -20, 0, 0},
                            {0.69f, -8, 0, 0}, {0.825f, 14, 0, 0}, {0.99f, 26, 0, 0}, {1.1f, 24, 0, 0}}, 0.55f)
            .both("shin", {{0, -4, 0, 0}, {0.154f, -14, 0, 0}, {0.275f, -6, 0, 0}, {0.44f, -6, 0, 0}, {0.55f, -24, 0, 0},
                           {0.69f, -58, 0, 0}, {0.825f, -52, 0, 0}, {0.99f, -10, 0, 0}, {1.1f, -4, 0, 0}}, 0.55f)
            .both("foot", {{0, -2, 0, 0}, {0.154f, -4, 0, 0}, {0.275f, 2, 0, 0}, {0.44f, 12, 0, 0}, {0.55f, 6, 0, 0},
                           {0.69f, -4, 0, 0}, {0.825f, 0, 0, 0}, {0.99f, 2, 0, 0}, {1.1f, -2, 0, 0}}, 0.55f)
            .both("upperarm_01", {{0, -20, 0, 3}, {0.55f, 22, 0, 3}, {1.1f, -20, 0, 3}}, 0.55f)
            .both("forearm_01", {{0, 14, 0, 0}, {0.55f, 34, 0, 0}, {1.1f, 14, 0, 0}}, 0.55f)
            .build());

        /* --- jog: 0.72 s cycle, a short flight between steps, elbows held bent ------------- */
        out.push_back(ClipBuilder("jog", 0.72f, true)
            .root({{0, 0, 0, 0, 0, 0.010f, 0}, {0.18f, 0, 0, 0, 0, -0.045f, 0}, {0.36f, 0, 0, 0, 0, 0.010f, 0},
                   {0.54f, 0, 0, 0, 0, -0.045f, 0}, {0.72f, 0, 0, 0, 0, 0.010f, 0}})
            .joint("hips", {{0, 0, 6.0f, 0}, {0.36f, 0, -6.0f, 0}, {0.72f, 0, 6.0f, 0}})
            .joint("spine_02", {{0, -7.0f, -6.0f, 0}, {0.36f, -7.0f, 6.0f, 0}, {0.72f, -7.0f, -6.0f, 0}})
            .both("thigh", {{0, 38, 0, 0}, {0.18f, 6, 0, 0}, {0.36f, -30, 0, 0}, {0.47f, -6, 0, 0}, {0.56f, 24, 0, 0}, {0.72f, 38, 0, 0}}, 0.36f)
            .both("shin", {{0, -22, 0, 0}, {0.18f, -36, 0, 0}, {0.36f, -20, 0, 0}, {0.47f, -92, 0, 0}, {0.56f, -88, 0, 0},
                           {0.66f, -44, 0, 0}, {0.72f, -22, 0, 0}}, 0.36f)
            .both("foot", {{0, 2, 0, 0}, {0.18f, 14, 0, 0}, {0.36f, -22, 0, 0}, {0.47f, -12, 0, 0}, {0.72f, 2, 0, 0}}, 0.36f)
            .both("upperarm_01", {{0, -34, 0, 5}, {0.36f, 32, 0, 5}, {0.72f, -34, 0, 5}}, 0.36f)
            .both("forearm_01", {{0, 72, 0, 0}, {0.36f, 88, 0, 0}, {0.72f, 72, 0, 0}}, 0.36f)
            .build());

        /* --- run: 0.62 s cycle, long stride, heel to the seat in the swing, forward lean ---- */
        out.push_back(ClipBuilder("run", 0.62f, true)
            .root({{0, 0, 0, 0, 0, 0.020f, 0}, {0.155f, 0, 0, 0, 0, -0.050f, 0}, {0.31f, 0, 0, 0, 0, 0.020f, 0},
                   {0.465f, 0, 0, 0, 0, -0.050f, 0}, {0.62f, 0, 0, 0, 0, 0.020f, 0}})
            .joint("hips", {{0, 0, 8.0f, 0}, {0.31f, 0, -8.0f, 0}, {0.62f, 0, 8.0f, 0}})
            .joint("spine_02", {{0, -11.0f, -8.0f, 0}, {0.31f, -11.0f, 8.0f, 0}, {0.62f, -11.0f, -8.0f, 0}})
            .both("thigh", {{0, 50, 0, 0}, {0.155f, 8, 0, 0}, {0.31f, -36, 0, 0}, {0.40f, -10, 0, 0}, {0.48f, 30, 0, 0}, {0.62f, 50, 0, 0}}, 0.31f)
            .both("shin", {{0, -28, 0, 0}, {0.155f, -42, 0, 0}, {0.31f, -24, 0, 0}, {0.40f, -104, 0, 0}, {0.48f, -110, 0, 0},
                           {0.57f, -60, 0, 0}, {0.62f, -28, 0, 0}}, 0.31f)
            .both("foot", {{0, 0, 0, 0}, {0.155f, 14, 0, 0}, {0.31f, -28, 0, 0}, {0.40f, -14, 0, 0}, {0.62f, 0, 0, 0}}, 0.31f)
            .both("upperarm_01", {{0, -50, 0, 6}, {0.31f, 44, 0, 6}, {0.62f, -50, 0, 6}}, 0.31f)
            .both("forearm_01", {{0, 84, 0, 0}, {0.31f, 98, 0, 0}, {0.62f, 84, 0, 0}}, 0.31f)
            .build());

        /* --- squat: one repetition. The hips go down and back by what the bent legs take away,
               so the feet stay flat on the floor: at the bottom the thigh is level (95 degrees
               from hanging), the shin leans 25 degrees over the foot. ------------------------ */
        out.push_back(ClipBuilder("squat", 2.4f, false)
            .root({{0, 0, 0, 0, 0, 0, 0}, {0.6f, 0, 0, 0, 0, -0.179f, 0.182f}, {1.2f, 0, 0, 0, 0, -0.508f, 0.247f},
                   {1.8f, 0, 0, 0, 0, -0.179f, 0.182f}, {2.4f, 0, 0, 0, 0, 0, 0}})
            .joint("hips", {{0, 0, 0, 0}, {0.6f, -11, 0, 0}, {1.2f, -20, 0, 0}, {1.8f, -11, 0, 0}, {2.4f, 0, 0, 0}})
            .joint("spine_02", {{0, 0, 0, 0}, {0.6f, -9, 0, 0}, {1.2f, -18, 0, 0}, {1.8f, -9, 0, 0}, {2.4f, 0, 0, 0}})
            .joint("head", {{0, 0, 0, 0}, {1.2f, 16, 0, 0}, {2.4f, 0, 0, 0}})
            .both("thigh", {{0, 0, 0, 0}, {0.6f, 61, 0, 5}, {1.2f, 115, 0, 9}, {1.8f, 61, 0, 5}, {2.4f, 0, 0, 0}})
            .both("shin", {{0, 0, 0, 0}, {0.6f, -70, 0, 0}, {1.2f, -120, 0, 0}, {1.8f, -70, 0, 0}, {2.4f, 0, 0, 0}})
            .both("foot", {{0, 0, 0, 0}, {0.6f, 20, 0, 0}, {1.2f, 25, 0, 0}, {1.8f, 20, 0, 0}, {2.4f, 0, 0, 0}})
            .both("upperarm_01", {{0, 0, 0, 3}, {0.6f, 42, 0, 6}, {1.2f, 92, 0, 8}, {1.8f, 42, 0, 6}, {2.4f, 0, 0, 3}})
            .both("forearm_01", {{0, 8, 0, 0}, {1.2f, 12, 0, 0}, {2.4f, 8, 0, 0}})
            .build());

        /* --- jump: crouch, launch off the toes, tuck, land on bent legs, settle ------------- */
        out.push_back(ClipBuilder("jump", 1.6f, false)
            .root({{0, 0, 0, 0, 0, 0, 0}, {0.30f, 0, 0, 0, 0, -0.175f, 0.121f}, {0.50f, 0, 0, 0, 0, 0.060f, 0.020f},
                   {0.75f, 0, 0, 0, 0, 0.420f, 0}, {1.00f, 0, 0, 0, 0, -0.091f, 0.081f}, {1.30f, 0, 0, 0, 0, -0.010f, 0.010f},
                   {1.60f, 0, 0, 0, 0, 0, 0}})
            .joint("hips", {{0, 0, 0, 0}, {0.30f, -14, 0, 0}, {0.50f, 0, 0, 0}, {0.75f, 2, 0, 0}, {1.00f, -10, 0, 0}, {1.30f, -3, 0, 0}, {1.60f, 0, 0, 0}})
            .joint("spine_02", {{0, 0, 0, 0}, {0.30f, -10, 0, 0}, {0.50f, 4, 0, 0}, {0.75f, 0, 0, 0}, {1.00f, -8, 0, 0}, {1.30f, -2, 0, 0}, {1.60f, 0, 0, 0}})
            .both("thigh", {{0, 0, 0, 0}, {0.30f, 60, 0, 4}, {0.50f, -4, 0, 0}, {0.75f, 16, 0, 0}, {1.00f, 42, 0, 4}, {1.30f, 10, 0, 0}, {1.60f, 0, 0, 0}})
            .both("shin", {{0, 0, 0, 0}, {0.30f, -72, 0, 0}, {0.50f, 0, 0, 0}, {0.75f, -28, 0, 0}, {1.00f, -52, 0, 0}, {1.30f, -14, 0, 0}, {1.60f, 0, 0, 0}})
            .both("foot", {{0, 0, 0, 0}, {0.30f, 26, 0, 0}, {0.50f, -34, 0, 0}, {0.75f, -20, 0, 0}, {1.00f, 20, 0, 0}, {1.30f, 4, 0, 0}, {1.60f, 0, 0, 0}})
            .both("upperarm_01", {{0, 0, 0, 3}, {0.30f, -38, 0, 6}, {0.50f, 110, 0, 10}, {0.75f, 140, 0, 14}, {1.00f, 30, 0, 8}, {1.30f, 6, 0, 4}, {1.60f, 0, 0, 3}})
            .both("forearm_01", {{0, 8, 0, 0}, {0.30f, 20, 0, 0}, {0.50f, 14, 0, 0}, {0.75f, 10, 0, 0}, {1.00f, 24, 0, 0}, {1.60f, 8, 0, 0}})
            .build());

        /* --- wave: the _L arm up beside the head, the forearm and hand swinging -------------- */
        out.push_back(ClipBuilder("wave", 2.2f, true)
            .joint("upperarm_01_L", {{0, 20, 0, 108}, {2.2f, 20, 0, 108}})
            .joint("forearm_01_L", {{0, 12, 0, 44}, {0.55f, 12, 0, 62}, {1.1f, 12, 0, 44}, {1.65f, 12, 0, 62}, {2.2f, 12, 0, 44}})
            .joint("hand_L", {{0, 0, 0, -16}, {0.55f, 0, 0, 16}, {1.1f, 0, 0, -16}, {1.65f, 0, 0, 16}, {2.2f, 0, 0, -16}})
            .joint("upperarm_01_R", {{0, 0, 0, -3}, {2.2f, 0, 0, -3}})
            .joint("forearm_01_R", {{0, 8, 0, 0}, {2.2f, 8, 0, 0}})
            .joint("head", {{0, 0, -5, -3}, {1.1f, 0, -7, -4}, {2.2f, 0, -5, -3}})
            .joint("spine_02", {{0, 0, 0, -2}, {2.2f, 0, 0, -2}})
            .build());

        /* --- punch: a straight with the _L hand from a guard, driven by the hips ------------- */
        out.push_back(ClipBuilder("punch", 0.9f, false)
            .root({{0, 0, 0, 0, 0, -0.011f, 0.015f}, {0.9f, 0, 0, 0, 0, -0.011f, 0.015f}})
            .joint("hips", {{0, 0, 0, 0}, {0.14f, 0, -8, 0}, {0.32f, 0, 20, 0}, {0.46f, 0, 18, 0}, {0.9f, 0, 0, 0}})
            .joint("spine_02", {{0, -3, 0, 0}, {0.14f, -3, -6, 0}, {0.32f, -5, 14, 0}, {0.46f, -5, 12, 0}, {0.9f, -3, 0, 0}})
            .joint("upperarm_01_L", {{0, 25, 0, 12}, {0.14f, 10, 0, 14}, {0.32f, 86, 0, 4}, {0.46f, 84, 0, 4}, {0.9f, 25, 0, 12}})
            .joint("forearm_01_L", {{0, 118, 0, 0}, {0.14f, 125, 0, 0}, {0.32f, 6, 0, 0}, {0.46f, 10, 0, 0}, {0.9f, 118, 0, 0}})
            .joint("upperarm_01_R", {{0, 25, 0, -12}, {0.32f, 15, 0, -14}, {0.9f, 25, 0, -12}})
            .joint("forearm_01_R", {{0, 118, 0, 0}, {0.32f, 128, 0, 0}, {0.9f, 118, 0, 0}})
            .both("thigh", {{0, 10, 0, 6}, {0.9f, 10, 0, 6}})
            .both("shin", {{0, -18, 0, 0}, {0.9f, -18, 0, 0}})
            .both("foot", {{0, 8, 0, 0}, {0.9f, 8, 0, 0}})
            .build());

        /* --- kick: a front kick with the _R leg: knee up, snap out, knee back, down ---------- */
        out.push_back(ClipBuilder("kick", 1.1f, false)
            .joint("hips", {{0, 0, 0, 0}, {0.42f, 0, -6, 0}, {1.1f, 0, 0, 0}})
            .joint("spine_02", {{0, 0, 0, 0}, {0.20f, 6, 0, 0}, {0.42f, 12, 0, 0}, {0.62f, 6, 0, 0}, {1.1f, 0, 0, 0}})
            .joint("thigh_R", {{0, 0, 0, 0}, {0.20f, 78, 0, -4}, {0.42f, 88, 0, -4}, {0.62f, 70, 0, -4}, {1.1f, 0, 0, 0}})
            .joint("shin_R", {{0, 0, 0, 0}, {0.20f, -100, 0, 0}, {0.42f, -10, 0, 0}, {0.62f, -90, 0, 0}, {1.1f, 0, 0, 0}})
            .joint("foot_R", {{0, 0, 0, 0}, {0.20f, -10, 0, 0}, {0.42f, -24, 0, 0}, {0.62f, -10, 0, 0}, {1.1f, 0, 0, 0}})
            .joint("thigh_L", {{0, 0, 0, 0}, {0.42f, 6, 0, 0}, {1.1f, 0, 0, 0}})
            .joint("shin_L", {{0, 0, 0, 0}, {0.42f, -12, 0, 0}, {1.1f, 0, 0, 0}})
            .joint("foot_L", {{0, 0, 0, 0}, {0.42f, 6, 0, 0}, {1.1f, 0, 0, 0}})
            .both("upperarm_01", {{0, 0, 0, 3}, {0.20f, 22, 0, 20}, {0.62f, 22, 0, 20}, {1.1f, 0, 0, 3}})
            .both("forearm_01", {{0, 8, 0, 0}, {0.20f, 100, 0, 0}, {0.62f, 100, 0, 0}, {1.1f, 8, 0, 0}})
            .build());

        /* --- pushup: a plank on hands and toes, the elbows bending to lower the chest -------- */
        out.push_back(ClipBuilder("pushup", 2.0f, true)
            .root({{0, -70, 0, 0, 0, 0.120f, 0}, {1.0f, -80, 0, 0, 0, 0.135f, 0}, {2.0f, -70, 0, 0, 0, 0.120f, 0}})
            .joint("neck_01", {{0, 10, 0, 0}, {2.0f, 10, 0, 0}})
            .joint("head", {{0, 16, 0, 0}, {2.0f, 16, 0, 0}})
            .both("upperarm_01", {{0, 70, 0, 6}, {1.0f, 12, 0, 22}, {2.0f, 70, 0, 6}})
            .both("forearm_01", {{0, 4, 0, 0}, {1.0f, 74, 0, 0}, {2.0f, 4, 0, 0}})
            .both("hand", {{0, 78, 0, 0}, {1.0f, 84, 0, 0}, {2.0f, 78, 0, 0}})
            .both("foot", {{0, 36, 0, 0}, {2.0f, 36, 0, 0}})
            .build());

        /* --- t_pose / a_pose: the arms straight out, and half way down ---------------------- */
        out.push_back(ClipBuilder("t_pose", 0.1f, false)
            .both("upperarm_01", {{0, 0, 0, 76}})
            .build());
        out.push_back(ClipBuilder("a_pose", 0.1f, false)
            .both("upperarm_01", {{0, 0, 0, 31}})
            .build());

        /* --- sit: on a seat at knee height, feet where they stood, hands on the thighs ------- */
        out.push_back(ClipBuilder("sit", 0.1f, false)
            .root({{0, 0, 0, 0, 0, -0.400f, 0.414f}})
            .joint("hips", {{0, -4, 0, 0}})
            .both("thigh", {{0, 90, 0, 4}})
            .both("shin", {{0, -88, 0, 0}})
            .both("foot", {{0, 2, 0, 0}})
            .both("upperarm_01", {{0, 24, 0, 5}})
            .both("forearm_01", {{0, 62, 0, 0}})
            .build());

        /* --- turn_360: the hips make a full turn, shoulders and head a beat behind ---------- */
        out.push_back(ClipBuilder("turn_360", 2.0f, true)
            .joint("hips", {{0, 0, 0, 0}, {0.5f, 0, 90, 0}, {1.0f, 0, 180, 0}, {1.5f, 0, 270, 0}, {2.0f, 0, 360, 0}})
            .joint("spine_02", {{0, 0, -10, 0}, {1.0f, 0, 10, 0}, {2.0f, 0, -10, 0}})
            .joint("head", {{0, 0, 24, 0}, {1.0f, 0, -24, 0}, {2.0f, 0, 24, 0}})
            .build());

        /* --- dance: a two-step groove. Weight from one leg to the other, the free knee soft,
               elbows bent and the hands trading places, down on the beat. --------------------- */
        out.push_back(ClipBuilder("dance", 2.4f, true)
            .root({{0, 0, 0, 0, 0.040f, -0.030f, 0}, {0.6f, 0, 0, 0, 0, -0.006f, 0}, {1.2f, 0, 0, 0, -0.040f, -0.030f, 0},
                   {1.8f, 0, 0, 0, 0, -0.006f, 0}, {2.4f, 0, 0, 0, 0.040f, -0.030f, 0}})
            .joint("hips", {{0, 0, -8, -5}, {0.6f, 0, 0, 0}, {1.2f, 0, 8, 5}, {1.8f, 0, 0, 0}, {2.4f, 0, -8, -5}})
            .joint("spine_02", {{0, -3, 6, 4}, {0.6f, -3, 0, 0}, {1.2f, -3, -6, -4}, {1.8f, -3, 0, 0}, {2.4f, -3, 6, 4}})
            .joint("head", {{0, -4, 5, 3}, {0.6f, 0, 0, 0}, {1.2f, -4, -5, -3}, {1.8f, 0, 0, 0}, {2.4f, -4, 5, 3}})
            .both("thigh", {{0, 12, 0, 7}, {0.6f, 5, 0, 3}, {1.2f, 3, 0, -3}, {1.8f, 5, 0, 3}, {2.4f, 12, 0, 7}}, 1.2f)
            .both("shin", {{0, -28, 0, 0}, {0.6f, -12, 0, 0}, {1.2f, -8, 0, 0}, {1.8f, -12, 0, 0}, {2.4f, -28, 0, 0}}, 1.2f)
            .both("foot", {{0, 16, 0, 0}, {0.6f, 7, 0, 0}, {1.2f, 5, 0, 0}, {1.8f, 7, 0, 0}, {2.4f, 16, 0, 0}}, 1.2f)
            .both("upperarm_01", {{0, 34, 0, 30}, {0.6f, 12, 0, 22}, {1.2f, -4, 0, 18}, {1.8f, 12, 0, 22}, {2.4f, 34, 0, 30}}, 1.2f)
            .both("forearm_01", {{0, 98, 0, 0}, {0.6f, 82, 0, 0}, {1.2f, 70, 0, 0}, {1.8f, 82, 0, 0}, {2.4f, 98, 0, 0}}, 1.2f)
            .build());

        /* --- hold_hat: the _L hand up to the brim of a hat, the elbow out. Made to be laid
               over a walk on that arm alone. ------------------------------------------------- */
        out.push_back(ClipBuilder("hold_hat", 3.0f, true)
            .joint("upperarm_01_L", {{0, 15, 0, 101}, {1.5f, 17, 0, 104}, {3.0f, 15, 0, 101}})
            .joint("forearm_01_L", {{0, 8, 0, 118}, {1.5f, 8, 0, 121}, {3.0f, 8, 0, 118}})
            .joint("hand_L", {{0, 0, 0, 10}, {3.0f, 0, 0, 10}})
            .build());

        return out;
    }();
    return clips;
}

} // namespace

const std::vector<Clip>& builtinClips() { return library(); }

namespace {
/* Order matches the af_clip enum in afcore.h EXACTLY, one entry per enum value. The test harness
   asserts id -> name -> library entry, so the enum, this table and the authored clips cannot
   drift apart (an earlier revision had jog and run transposed here and the ids resolved to the
   wrong clip without any test noticing). */
const char* const kClipOrder[] = {
        "rest", "idle", "breathe", "walk", "run", "jog", "squat", "jump", "wave",
    "punch", "kick", "t_pose", "a_pose", "sit", "turn_360", "pushup", "dance", "hold_hat"
};
const int kClipOrderCount = static_cast<int>(sizeof(kClipOrder) / sizeof(kClipOrder[0]));
} // namespace

const char* clipNameForId(int afClipId) {
    if (afClipId < 0 || afClipId >= kClipOrderCount) return nullptr;
    return kClipOrder[afClipId];
}

int clipIndexForId(int afClipId) {
    /* "rest" is served by the identity pose and has no authored track data, so it maps to -1. */
    const char* name = clipNameForId(afClipId);
    if (!name) return -1;
    const std::string want = name;
    if (want == "rest") return -1;              /* rest = all identity, no track needed */
    const std::vector<Clip>& lib = builtinClips();
    for (size_t i = 0; i < lib.size(); ++i) if (lib[i].name == want) return static_cast<int>(i);
    return -1;
}

} // namespace af
