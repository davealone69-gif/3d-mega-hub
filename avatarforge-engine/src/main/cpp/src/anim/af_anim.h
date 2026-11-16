/*
 * af_anim.h -- the animation core: clips, sampling, blending, state machine, layers, morph/facial
 *              control, lip-sync and retargeting.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * Before this module the project had two unrelated halves of an animation system and no bridge
 * between them:
 *
 *   - `af_rig.h` owns a real 75-joint skeleton, LBS skinning, 52 ARKit blendshape channels and a
 *     library of 16 *authored* clips (af::Clip / af::Track / af::Keyframe, sampled by
 *     af_engine_sample_clip). Those clips only ever drove the engine's OWN generated rig, and only
 *     one at a time: there was no blending, no transition, no layering and no facial track.
 *   - `af_glb_read.h` imports a glTF 2.0 file including its skins, inverse bind matrices and
 *     animation samplers (LINEAR/STEP/CUBICSPLINE) -- and then `af_engine`'s importer threw the
 *     result away, logging "file contains a skeleton; it is not applied" and "file contains
 *     animation clips; they are not applied". Morph-target ("weights") channels were dropped at
 *     parse time outright.
 *
 * So an imported animated GLB could be *measured* but never *played* by this engine, and the
 * authored library could never be applied to anything but the generated avatar. This module closes
 * both gaps with one implementation that both renderer backends consume:
 *
 * The import half of that claim only became true when `af_engine`'s importer started adopting the
 * file's own skeleton and clips into a Player and CPU-skinning from them -- see `af_engine_imported_*`
 * in afcore.h and `imported_animation_test.cpp`. This module supplied the machinery, but for a while
 * the importer still declined to use it: imported models stayed static in their bind pose while every
 * test in here passed green. A capability nobody calls is not a closed gap, so if you extend this
 * module, check which backends actually reach it.
 *
 *   af::anim::Player  ->  af::anim::Pose  ->  { bone palette, morph weights }
 *                                              |                       |
 *                            AvatarForge GLES (CPU deform / GPU skin)  Filament (TransformManager
 *                                                                        + RenderableManager)
 *
 * WHAT IT DOES NOT DO
 * -------------------
 * No GPU work, no graphics headers, no Android headers. It compiles and is tested on the host
 * against real Khronos glTF sample assets (Fox.glb, CesiumMan.glb, AnimatedMorphCube.glb) and
 * against the engine's own generated rig. A wrong sample, a wrong blend or a wrong retarget is
 * caught here, on the host, before it can reach a phone.
 *
 * It also does not invent animation. A clip exists only if the imported file declares it or the
 * authored library contains it; `Clip::fromGlb` fails rather than synthesising motion, and
 * `FaceController` reports which requested channels are NOT present on the target asset instead of
 * pretending to drive them.
 *
 * CONVENTIONS (the engine's, unchanged): +Y up, -Z forward, metres, column-major Mat4, quaternion
 * stored {x,y,z,w} which is also glTF's component order. Parents are evaluated before children.
 *
 * ALLOCATION POLICY: every per-frame buffer lives in af::anim::Pose and is sized once by
 * Pose::allocate(). Steady-state advance()/sample()/blend() perform no heap allocation, which is
 * what makes the frame budget on a phone predictable. The host test suite asserts this with a
 * counting operator new rather than assuming it.
 */
#ifndef AF_ANIM_H
#define AF_ANIM_H

#include "../af_glb_read.h"
#include "../af_math.h"
#include "../af_rig.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace af {
namespace anim {

/* ======================================================================== interpolation / paths */

enum class Interp : uint8_t {
    Linear = 0,
    Step = 1,
    CubicSpline = 2,
    /* The easing the engine's own authored library uses (af::Track::sample in af_rig.cpp):
       smoothstep between keys, then slerp. Kept as a distinct mode rather than flattened to
       Linear, because that easing is a deliberate authored choice -- the af_rig.cpp comment calls
       out the "linear-slerp robotic snap" it exists to avoid -- and converting the 16 built-in
       clips to Linear would visibly change animations that already ship. glTF files never produce
       this mode; it is only used by Clip::fromAuthored. */
    Smoothstep = 3,
};

enum class Path : uint8_t {
    Translation = 0,
    Rotation = 1,
    Scale = 2,
    Weights = 3,      /* morph-target weights; the channel af_glb_read used to discard */
};

const char* interpName(Interp i);
const char* pathName(Path p);

/* Declared here because Skeleton::buildPalette takes one, and Pose is defined after Skeleton: the
   palette is derived from a pose, but its shape is a property of the skeleton. */
struct Pose;

/* ============================================================================ humanoid skeleton */

/*
 * The bone roles a retarget needs. Deliberately the small humanoid core (Hips..Hand) rather than
 * every joint: retargeting 75 engine joints onto a 19-joint Cesium rig cannot work bone-for-bone,
 * but it can work role-for-role, and unmapped joints then hold their rest pose instead of snapping.
 */
enum class Role : int {
    Hips = 0,
    Spine,
    Chest,
    Neck,
    Head,
    LeftUpperLeg,
    LeftLowerLeg,
    LeftFoot,
    RightUpperLeg,
    RightLowerLeg,
    RightFoot,
    LeftUpperArm,
    LeftLowerArm,
    LeftHand,
    RightUpperArm,
    RightLowerArm,
    RightHand,
    COUNT
};

const char* roleName(Role r);
bool roleIsLeftSide(Role r);
bool roleIsRightSide(Role r);
/** The role on the opposite side of the body, or `r` itself for midline roles. */
Role roleMirrored(Role r);

/* ------------------------------------------------------------------ matrix decomposition ------ */

/**
 * Splits a column-major TRS matrix into translation, rotation and scale, the inverse of
 * Mat4::trs(). Returns false when the matrix is not decomposable (a zero-length basis column or a
 * non-finite value), leaving the outputs untouched. Scale sign is preserved through the
 * determinant so a mirrored bone does not come back inside-out.
 */
bool decomposeTrs(const Mat4& m, Vec3* outT, Quat* outR, Vec3* outS);

/** Builds the quaternion whose rotation matrix is `r` (columns are the basis vectors). */
Quat quatFromRotationMatrix(const Vec3& col0, const Vec3& col1, const Vec3& col2);

/* ================================================================================== Skeleton === */

/*
 * A bindable skeleton: joint names, a parent array, the rest (bind) local TRS, optional inverse
 * bind matrices, a parents-before-children evaluation order, and the detected humanoid role of
 * each joint.
 *
 * Built from either a glTF skin (Skeleton::fromGlb) or the engine's generated rig
 * (Skeleton::fromRig). Both produce the same structure, which is what makes a clip authored for
 * one playable on the other through the retargeter.
 *
 * CLOSURE, AND WHY IT IS NOT JUST THE JOINT LIST
 * ----------------------------------------------
 * fromGlb() includes every ancestor node of every skin joint, not only the joints themselves. A
 * joint's animation channel writes its LOCAL transform relative to its node parent, and a joint's
 * world transform therefore depends on every node above it -- including nodes that are not joints
 * and carry their own transform (Fox.glb parents its `_rootJoint` to a plain `root` node). Building
 * the skeleton from the bare joint list would silently drop those intervening transforms and
 * produce a pose that is wrong by exactly that node's TRS. Closure nodes that are not skin joints
 * get jointToSkinSlot == -1 and take no palette slot; they exist only to carry the hierarchy.
 */
struct Skeleton {
    std::vector<std::string> names;
    std::vector<int> parents;              /* -1 for a root                          */
    std::vector<Vec3> restT;
    std::vector<Quat> restR;
    std::vector<Vec3> restS;
    std::vector<Mat4> inverseBind;         /* parallel to the joints; identity for a closure node that
                                              is not a skin joint, or when the source declared none */
    std::vector<int> order;                /* every joint exactly once, parents first */
    std::vector<Role> roles;               /* per joint; Role::COUNT when unmapped    */
    std::vector<int> roleToJoint;          /* size Role::COUNT; primary joint per role, -1 when absent */
    std::vector<int> roleChain;            /* flat, grouped by role: every joint carrying that role,
                                              shallowest first. roleChainOffset[r] indexes into it. */
    std::vector<int> roleChainOffset;      /* size Role::COUNT + 1 */
    std::vector<int> jointToSkinSlot;      /* skeleton index -> skin joint slot, -1 for a non-joint
                                              node pulled in only to complete the hierarchy */
    std::vector<int> sourceNode;           /* skeleton index -> glTF node index; -1 for a rig-built
                                              skeleton. Animation channels address nodes, so this is
                                              the map that binds an imported clip to the skeleton. */
    std::vector<int> skinSlotToJoint;      /* skin joint slot -> skeleton index */
    int meshNode = -1;                     /* node the skinned mesh hangs off, for the skinning space */
    Mat4 meshNodeInverse = Mat4::identity();
    bool hasInverseBind = false;
    std::string sourceName;                /* provenance for diagnostics: file/skin/rig */

    int count() const { return static_cast<int>(names.size()); }
    int skinSlotCount() const { return static_cast<int>(skinSlotToJoint.size()); }
    bool empty() const { return names.empty(); }
    int find(const std::string& name) const;
    /** Case/separator-insensitive lookup: "upperarm_01_L" matches "UpperArm_01_L". */
    int findFuzzy(const std::string& name) const;
    int jointForRole(Role r) const;
    /** Joints carrying role `r`, shallowest first. Empty when the role is absent. */
    std::vector<int> jointsForRole(Role r) const;

    /** True when names/parents/order agree, parents precede children in `order`, and there are no
     *  cycles. A skeleton that fails this must not be sampled: the world pass would read
     *  uninitialised parents. */
    bool validate(std::string* error) const;

    /** Rest-pose world position of a joint, used for limb-length and hip-height ratios. */
    Vec3 restWorldPosition(int joint) const;
    /** Distance from the joint to its first child, i.e. the bone length it drives. 0 for leaves. */
    float boneLength(int joint) const;
    /** How many of the Role::COUNT humanoid roles this skeleton resolved. Read-only counterpart to
     *  assignRoles(), so a caller can report coverage without re-running detection or const_casting. */
    int rolesMatched() const;

    /** World +Y of the Hips role in the rest pose; 0 when the role is absent. Drives retarget scale. */
    float hipHeight() const;

    /**
     * Writes the GPU bone palette: one matrix per skin joint slot, in slot order, computed as
     * glTF specifies -- inverse(meshNodeWorld) * jointWorld * inverseBindMatrices[slot].
     * Produces nothing when the skeleton carries no inverse bind matrices, because then there is
     * no correct palette to produce; the caller must fall back to CPU deformation or refuse.
     */
    void buildPalette(const Pose& pose, std::vector<Mat4>* palette) const;

    /** Detects humanoid roles from joint names plus hierarchy, filling roles/roleToJoint.
     *  Deterministic and side-aware; unmatched joints keep Role::COUNT. Returns how many of the
     *  Role::COUNT roles were found. */
    int assignRoles();

    static bool fromGlb(const GlbAsset& asset, int skinIndex, Skeleton* out, std::string* error);
    /** Builds a skeleton over the whole node graph with no skin and no inverse bind matrices. This
     *  is what an imported morph-only asset gets (AnimatedMorphCube has one node, two morph targets
     *  and no skeleton at all), so its weights channels can play through the same Player path
     *  instead of needing a second one. The result has hasInverseBind == false, so buildPalette()
     *  produces nothing and no skinning is claimed. */
    static bool fromGlbNodes(const GlbAsset& asset, Skeleton* out, std::string* error);
    static bool fromRig(const Rig& rig, Skeleton* out, std::string* error);
};

/* ====================================================================================== Pose === */

/*
 * One evaluated pose. All buffers are allocated once by allocate() and then reused, so the
 * per-frame path never touches the heap.
 *
 * `local`/`world`/`skin` are derived: call computeMatrices() after writing t/r/s. `skin` is the
 * bone palette a GPU skinning shader consumes (glTF's
 * inverse(meshNodeWorld) * jointWorld * inverseBind), and is left empty when the skeleton carries
 * no inverse bind matrices, because then there is no correct palette to produce.
 */
struct Pose {
    std::vector<Vec3> t;
    std::vector<Quat> r;
    std::vector<Vec3> s;
    std::vector<Mat4> local;
    std::vector<Mat4> world;
    std::vector<Mat4> skin;
    std::vector<float> morphs;        /* flat: morphCount() entries, or empty when the asset has none */
    Vec3 rootMotion{0.0f, 0.0f, 0.0f}; /* translation delta extracted this frame, already consumed */
    bool matricesValid = false;

    void allocate(const Skeleton& sk, int morphCount = 0);
    void resetToRest(const Skeleton& sk);
    void computeMatrices(const Skeleton& sk);
    int jointCount() const { return static_cast<int>(t.size()); }
    int morphCount() const { return static_cast<int>(morphs.size()); }
    bool isFinite() const;
};

/* =============================================================================== Clip / Event === */

/** A named moment in a clip (footstep, gesture peak). Fired once per playthrough by the Player. */
struct Event {
    float time = 0.0f;
    std::string name;
    float value = 0.0f;
};

/**
 * One animated target. For TRS paths `joint` addresses the skeleton and `components` is 3
 * (translation/scale) or 4 (rotation). For Weights, `morphOffset`/`morphCount` address the flat
 * morph array and the samples live in `weightValues` (keys * morphCount), because a mesh can have
 * more than four morph targets and Vec4 would silently truncate them.
 *
 * `values` holds one Vec4 per key for Linear/Step and THREE per key for CubicSpline, in glTF's
 * (in-tangent, value, out-tangent) order.
 */
struct Channel {
    int joint = -1;
    Path path = Path::Rotation;
    Interp interp = Interp::Linear;
    int components = 0;
    int morphOffset = 0;
    int morphCount = 0;
    std::vector<float> times;         /* seconds, ascending */
    std::vector<Vec4> values;
    std::vector<float> weightValues;  /* Weights path only */
};

/** Categories used by the animation library. Assigned from the clip's own name, never guessed
 *  from the character: a Fox "Walk" is a walk, and an unnamed clip stays Uncategorized. */
enum class Category : int {
    Uncategorized = 0,
    Idle,
    Walking,
    Running,
    Jumping,
    Dancing,
    Combat,
    Gestures,
    Emotions,
    Talking,
    Facial,
    COUNT
};

const char* categoryName(Category c);
/** Classifies a clip by name ("Survey" -> Idle, "Walk" -> Walking, "Run" -> Running, ...). */
Category categoryForClipName(const std::string& name);

struct Clip {
    std::string name;
    float duration = 0.0f;
    bool looping = true;
    Category category = Category::Uncategorized;
    int sourceAnimation = -1;          /* index into GlbAsset::animations, or -1 when authored */
    bool authored = false;             /* true for clips from af::builtinClips() */
    bool hasRootTranslation = false;   /* a translation channel on the root: root motion is meaningful */
    std::vector<Channel> channels;
    std::vector<Event> events;

    int morphChannelCount() const;
    /** Largest morph index this clip writes, +1; 0 when it animates no morph target. */
    int morphSpan() const;

    /**
     * Binds one imported glTF animation to a skeleton. Channels whose target node is not a joint
     * of the skeleton are reported in `skipped` and dropped rather than silently ignored -- that
     * is how a mesh-node morph animation survives on a skinned character.
     */
    static bool fromGlb(const GlbAsset& asset, int animationIndex, const Skeleton& sk,
                        Clip* out, std::vector<std::string>* skipped, std::string* error);

    /** Converts an authored af::Clip (af_rig.h) to this representation, resolving tracks by joint
     *  name against `sk`. Tracks whose joint is absent are listed in `skipped`. */
    static bool fromAuthored(const af::Clip& authored, const Skeleton& sk, Clip* out,
                             std::vector<std::string>* skipped);

    /** Binds an authored af::Clip to a FOREIGN skeleton by humanoid Role instead of by joint name.
     *  This is what makes the engine's own library usable on an imported rig that was named by someone
     *  else: the authored tracks are written against the generated rig's vocabulary ("hips", "thigh_L",
     *  "upperarm_01_L"), which that vocabulary maps to Role explicitly, and Role then resolves against
     *  whatever the target skeleton calls the same bone ("Hips", "LeftUpperLeg", "LeftUpperArm").
     *  Rotation channels only: the authored translations are scaled for a 1.75 m figure and there is no
     *  source rig here to derive a ratio from, so transferring them unscaled would visibly overshoot on a
     *  unit-normalised import. Every track that cannot be placed is listed in `skipped` with its reason,
     *  including the translations that were deliberately dropped. */
    static bool fromAuthoredByRole(const af::Clip& authored, const Skeleton& target, Clip* out,
                                   std::vector<std::string>* skipped);

    /** True when any channel writes a morph target. */
    bool hasMorphChannels() const { return morphChannelCount() > 0; }
};

/** Every distinct joint name the given authored library writes to, and the Role the binding table
 *  assigns it (Role::COUNT when the table does not cover the name). Exposed so a test can prove the
 *  table still covers the whole library instead of trusting it: a library that grows a new joint name
 *  and a table that does not know it would otherwise silently drop that track from every imported
 *  avatar, and the animation would still "work" - just with a limb that never moves. */
std::vector<std::string> authoredTrackNames(const std::vector<af::Clip>& library);
Role authoredRoleForTrackName(const std::string& name);

/** True for library entries that are single-key CALIBRATION poses rather than motion ('t_pose', 'a_pose').
 *  fromAuthoredByRole refuses them on a foreign skeleton and reports why: they describe the generated
 *  rig's rest SHAPE, which an imported file's bind pose already provides for itself. */
bool authoredClipIsCalibrationPose(const std::string& clipName);

/** The generated rig's rest bone direction for a Role, as the unit vector from that joint to its child in
 *  the joint's own local frame - the frame the authored library's rotations were written in. A zero vector
 *  means "no reliable axis" (Head and both Hands, whose first child is a jaw or a finger, not a limb), and
 *  fromAuthoredByRole then skips direction alignment for that joint. Exposed so a test can rebuild the real
 *  rig and prove these numbers have not gone stale. */
af::Vec3 authoredRestDirectionForRole(Role role);

/** Total morph targets across every mesh in an imported asset: the width of the flat morph array a
 *  Player allocates, and the count a weights channel's offset is measured against. */
int totalMorphTargetsForAsset(const GlbAsset& asset);

/* ============================================================================ sampling ========= */

/**
 * Per-channel key cursor. Sampling a 60 Hz timeline re-reads the same keyframe pair almost every
 * frame, so the search starts from where the previous frame left off instead of from zero. This is
 * the only mutable state in the sample path and it is why sampling is O(1) per channel per frame.
 */
struct SampleCache {
    std::vector<uint32_t> cursor;
    void bind(const Clip& clip);
    void reset();
};

/** Finds the key interval containing `t`. Returns the lower index; the upper is lower+1 when in
 *  range. Exposed for tests because the CUBICSPLINE tangent pairing depends on getting it right. */
int findKeyInterval(const std::vector<float>& times, float t, uint32_t* cursor);

/** Samples one TRS channel at `t` into `out` (Vec4; w unused for 3-component paths). */
void sampleChannelVec(const Channel& ch, float t, uint32_t* cursor, Vec4* out);

/**
 * Evaluates `clip` at `time` into `out`.
 *
 * `out` is first reset to the skeleton's rest pose, then every channel is applied. That is glTF's
 * semantics: an unanimated joint keeps its bind transform, so a 1-channel clip does not collapse
 * the rest of the body. `time` is clamped to [0, duration] -- looping is the Player's job, not the
 * sampler's, so that scrubbing and looping cannot disagree about what a given time means.
 */
void sampleClip(const Clip& clip, const Skeleton& sk, float time, Pose* out, SampleCache* cache);

/* ============================================================================== blending ======= */

/**
 * Which joints a layer affects. Index-parallel to the skeleton; 0 means the layer does not touch
 * that joint and the underlying pose shows through. Built once and reused.
 */
struct BoneMask {
    std::vector<uint8_t> include;
    void allocate(const Skeleton& sk);
    void setAll(bool value);
    void set(int joint, bool value);
    bool test(int joint) const;
    int includedCount() const;
    /** Everything from the Chest role up, plus both arm chains: the usual "gesture over locomotion"
     *  split. Falls back to all-joints when the Chest role is absent. */
    static BoneMask upperBody(const Skeleton& sk);
    /** Hips, spine and both leg chains. */
    static BoneMask lowerBody(const Skeleton& sk);
    /** A role and everything below it in the hierarchy. */
    static BoneMask fromRole(const Skeleton& sk, Role root, bool includeDescendants);
};

/** One weighted contributor to a blend. `weight` need not be pre-normalised. */
struct BlendEntry {
    const Pose* pose = nullptr;
    float weight = 1.0f;
};

/**
 * Weighted blend of N poses (N >= 1). Weights are normalised over the entries actually supplied,
 * so a caller can pass a crossfade pair whose weights do not yet sum to 1 without a discontinuity.
 * Rotations blend by nlerp over the shortest arc -- slerp's exactness is not worth its cost inside
 * a per-joint inner loop at 60 Hz, and the two agree to well under a degree for the small angular
 * differences a crossfade actually produces.
 */
void blendPoses(const std::vector<BlendEntry>& entries, Pose* out);

/**
 * Overlays `overlay` onto `base` at `weight`, restricted to `mask`. Joints outside the mask keep
 * `base` exactly. Used for layered animation: an upper-body gesture over lower-body locomotion.
 */
void blendMasked(const Pose& base, const Pose& overlay, float weight, const BoneMask& mask,
                 Pose* out);

/**
 * Additive layer: `out = base * (rest^-1 * additive)^weight` per joint, restricted to `mask`.
 * The additive clip is expressed relative to the rest pose, which is what makes a small authored
 * "breathe" or "nod" clip usable on top of any base animation.
 */
void blendAdditive(const Skeleton& sk, const Pose& base, const Pose& additive, float weight,
                   const BoneMask& mask, Pose* out);

/** Linear blend of two morph-weight arrays, writing into `out` (resized to the longer input). */
void blendMorphWeights(const std::vector<float>& a, const std::vector<float>& b, float w,
                       std::vector<float>* out);

/** Weighted blend of N morph-weight arrays. Entries with a null array are skipped. */
void blendMorphWeightEntries(const std::vector<BlendEntry>& entries,
                             const std::vector<const std::vector<float>*>& arrays,
                             std::vector<float>* out);

/* ========================================================================= state machine ======= */

/**
 * Idle -> Walk -> Run and back, with a crossfade on every edge.
 *
 * This is a deliberately small state machine: named states, each bound to one clip, plus explicit
 * transitions carrying a crossfade duration. It owns *time*, not poses: advance() moves the current
 * and outgoing clip clocks and the crossfade alpha, and output() says which two clips to sample and
 * how to weight them. The Player does the sampling and calls blendPoses(). Keeping those separate
 * is what lets the same machine drive the Filament backend, where the two clips are sampled by
 * gltfio and blended by Animator::applyCrossFade instead of by us.
 */
class StateMachine {
public:
    struct Output {
        int clipA = -1;          /* current (destination) clip; alpha == 1 means only this one */
        float timeA = 0.0f;
        int clipB = -1;          /* outgoing clip; -1 when not transitioning */
        float timeB = 0.0f;
        float alpha = 1.0f;      /* 0 -> fully clipB, 1 -> fully clipA */
        bool transitioning = false;
    };

    /** Adds a state bound to `clipIndex`. Returns the state id, or -1 on a duplicate name. */
    int addState(const std::string& name, int clipIndex, bool looping = true);
    /** Adds an edge. A missing edge means the transition is not allowed, and requestState() says so
     *  rather than snapping. Returns false when either endpoint is unknown. */
    bool addTransition(const std::string& from, const std::string& to, float crossfadeSeconds);
    /** Allows every ordered pair with `crossfadeSeconds`, for a small all-connected machine. */
    void addAllTransitions(float crossfadeSeconds);

    int stateCount() const { return static_cast<int>(states_.size()); }
    const std::string& stateName(int state) const;
    int stateIndex(const std::string& name) const;
    int clipForState(int state) const;
    bool transitionExists(int from, int to, float* outCrossfade) const;

    /** Begins a transition to `name`. Returns false (and changes nothing) when the state is unknown
     *  or no transition edge exists -- a refused transition is reported, never faked. */
    bool requestState(const std::string& name);
    bool requestState(int state);

    void advance(float dt, const std::vector<Clip>& clips);
    void reset(int state);

    int currentState() const { return current_; }
    const std::string& currentStateName() const;
    int previousState() const { return previous_; }
    bool isTransitioning() const { return transitionTime_ >= 0.0f; }
    float transitionProgress() const;
    float transitionDuration() const { return transitionDuration_; }
    float stateTime() const { return currentTime_; }
    Output output() const;

private:
    struct State { std::string name; int clip = -1; bool looping = true; };
    struct Edge { int from = -1; int to = -1; float crossfade = 0.25f; };
    std::vector<State> states_;
    std::vector<Edge> edges_;
    int current_ = -1;
    int previous_ = -1;
    float currentTime_ = 0.0f;
    float previousTime_ = 0.0f;
    float transitionTime_ = -1.0f;   /* < 0 means not transitioning */
    float transitionDuration_ = 0.0f;
};

/* =========================================================================== root motion ======= */

/**
 * Extracts the root joint's translation delta between two sampled times so locomotion can move the
 * character instead of sliding in place.
 *
 * Reported honestly: it is only meaningful when the clip actually animates the root's translation
 * (Clip::hasRootTranslation). A rotation-only clip -- Fox's three cycles drive 20 rotations and a
 * single root translation -- yields whatever that channel contains and no more. The caller can
 * query `Clip::hasRootTranslation` and decide whether to consume the delta or keep the character
 * pinned, which is what the UI does.
 */
struct RootMotion {
    bool enabled = false;
    Vec3 accumulated{0.0f, 0.0f, 0.0f};
    Vec3 consume();                 /* returns and zeroes the accumulated delta */
    void reset();
};

/**
 * Root translation between two times, computed by walking only the root joint's ancestor chain and
 * sampling the translation/rotation channels on it. It deliberately does NOT build a full Pose:
 * root motion is requested every frame, and two throwaway Pose allocations per frame would break the
 * module's allocation policy for a value that needs at most a handful of channels.
 */
Vec3 rootTranslationDelta(const Skeleton& sk, const Clip& clip, float fromTime, float toTime,
                          SampleCache* cache);

/** The joint root motion is measured at: the joint whose translation actually carries the character
 *  through space, or -1 when this clip translates nothing. Prefers the deepest joint in the Hips
 *  role chain that the clip translates (Fox drives `b_Hip_01`, three joints below its scene root),
 *  falling back to the shallowest translated joint so a rig with no recognised Hips still measures
 *  travel rather than reporting none. Both `Clip::hasRootTranslation` and `rootTranslationDelta`
 *  consult this, so the flag and the measurement cannot disagree. */
int rootMotionJoint(const Skeleton& sk, const Clip& clip);

/* ============================================================================ facial / morphs == */

/** Lip-sync visemes. The 15-mouth-set used by most real-time lip-sync rigs (Oculus/Meta style),
 *  mapped onto morph targets by name when the asset has them. */
enum class Viseme : int {
    Silence = 0, PP, FF, TH, DD, KK, CH, SS, NN, RR, AA, E, I, O, U, COUNT
};
const char* visemeName(Viseme v);
/** Grapheme -> viseme. A spelling-based approximation, and labelled as such: it is not a phoneme
 *  recogniser, and the UI says "approximate" wherever it is offered. */
Viseme visemeForChar(char c);

/** Named facial expressions, defined as ARKit blendshape weight sets. */
enum class Expression : int {
    Neutral = 0, Happy, Sad, Angry, Surprised, Fear, Disgust, COUNT
};
const char* expressionName(Expression e);
/** Writes the ARKit weights for `e` scaled by `amount` into `out` (52 entries). */
void expressionWeights(Expression e, float amount, std::vector<float>* out);

/** A viseme expressed as ARKit jaw/mouth weights, so lip-sync works on an ARKit-named face. */
void visemeWeights(Viseme v, float amount, std::vector<float>* out);

/**
 * Drives the morph targets a loaded asset ACTUALLY has.
 *
 * configure() is given the real target names (from mesh.extras.targetNames, from Filament's
 * getMorphTargetNames, or empty when the file declares none). Requests for a channel the asset does
 * not have are recorded in unbound() and reported to the user; they are never silently treated as
 * success. Assets with no names at all are still drivable by index, which is how
 * AnimatedMorphCube -- two targets, no declared names -- plays.
 */
class FaceController {
public:
    /** Prepares for `morphCount` targets with the given names (names may be shorter/empty). */
    void configure(int morphCount, const std::vector<std::string>& names);
    void clear();

    int morphCount() const { return static_cast<int>(weights_.size()); }
    const std::vector<float>& weights() const { return weights_; }
    const std::vector<std::string>& names() const { return names_; }
    /** Channels requested by name that this asset does not provide. */
    const std::vector<std::string>& unbound() const { return unbound_; }
    bool hasNamedTargets() const;

    int indexOf(const std::string& name) const;
    /** Returns false when the name is absent (and records it in unbound_ once). */
    bool setWeight(const std::string& name, float w);
    bool setWeightAt(int index, float w);
    float weightAt(int index) const;
    float weightOf(const std::string& name) const;

    /** Applies an ARKit expression preset. Only channels present on this asset move; the rest are
     *  reported through unbound(). Returns how many channels were actually written. */
    int applyExpression(Expression e, float amount);
    /** Applies one viseme. Returns how many channels were actually written. */
    int applyViseme(Viseme v, float amount);
    /** Blends an ARKit weight vector (52 entries, af_rig.h order) onto the current weights at
     *  `amount`, for assets that carry ARKit names. Returns channels written. */
    int applyArkitWeights(const std::vector<float>& arkit, float amount);
    void setAll(float w);
    void resetWeights();

    /* ---- autonomous blink ---- */
    void setBlinkEnabled(bool enabled);
    bool blinkEnabled() const { return blinkEnabled_; }
    void setBlinkInterval(float minSeconds, float maxSeconds);
    void setBlinkDuration(float seconds);
    /** Advances the blink clock and writes eyeBlinkLeft/Right when those targets exist.
     *  Returns true during a blink. Deterministic given the same seed. */
    bool updateBlink(float dt);
    /** Forces one blink now; used by the UI's "Blink" button. */
    void triggerBlink();
    float blinkPhase() const { return blinkPhase_; }
    /** Reseeds the blink generator AND restarts the schedule, so the same seed always produces the
     *  same blink timeline no matter what the controller did before. Without the restart a seed only
     *  affected future draws and left the next blink wherever the clock happened to be, which made
     *  "reproducible from a seed" false. The next interval is drawn lazily on the following update,
     *  so calling this before or after setBlinkInterval() gives the same result. Current eyelid
     *  weights are untouched: reseeding is not a request to move the face. */
    void setBlinkSeed(uint32_t seed);

    /* ---- lip-sync ---- */
    /** Enables viseme driving from a LipSyncTrack. */
    void setLipSyncEnabled(bool enabled);
    bool lipSyncEnabled() const { return lipSyncEnabled_; }

private:
    void noteUnbound(const std::string& name);
    std::vector<float> weights_;
    std::vector<std::string> names_;
    std::vector<int> arkitToMorph_;      /* 52 entries; -1 when absent */
    std::vector<std::string> unbound_;
    bool blinkEnabled_ = false;
    void drawNextBlink();                /* re-draw blinkNext_ from the seeded stream */
    float blinkTimer_ = 0.0f;
    float blinkInterval_ = 3.5f;
    float blinkNext_ = 0.0f;             /* seconds until the next blink; <= 0 means "draw on next
                                            update". Drawn lazily so that setBlinkSeed() and
                                            setBlinkInterval() can be called in either order and still
                                            produce the same timeline. */
    float blinkDuration_ = 0.14f;
    float blinkPhase_ = 0.0f;            /* 0 = open, 1 = fully closed */
    bool blinking_ = false;
    uint32_t rng_ = 0x9E3779B9u;
    bool lipSyncEnabled_ = false;
};

/** One viseme at one time. */
struct VisemeKey {
    float time = 0.0f;
    Viseme viseme = Viseme::Silence;
    float amount = 1.0f;
};

/**
 * A lip-sync timeline. Two constructors, both honest about what they are:
 *   - buildFromKeys(): an explicit timeline, e.g. from a real phoneme aligner.
 *   - buildFromText(): a deterministic syllable-rate approximation from spelling. It does NOT
 *     analyse audio and does not claim to; the UI labels it "approximate text lip-sync".
 */
class LipSyncTrack {
public:
    void buildFromKeys(std::vector<VisemeKey> keys);
    void buildFromText(const std::string& text, float secondsPerViseme = 0.11f);
    /** Approximates visemes for a spoken duration, spreading them evenly. Used to drive an
     *  Android TextToSpeech utterance whose real timing is not observable. */
    void buildForDuration(const std::string& text, float totalSeconds);
    void clear();

    float duration() const { return duration_; }
    int keyCount() const { return static_cast<int>(keys_.size()); }
    const std::vector<VisemeKey>& keys() const { return keys_; }
    /** Viseme and amount at `t`; Silence outside the track. */
    Viseme sample(float t, float* outAmount) const;

private:
    std::vector<VisemeKey> keys_;
    float duration_ = 0.0f;
};

/* =========================================================================== retargeting ======= */

/**
 * A source->destination bone mapping plus the alignment factors that make the two skeletons
 * compatible enough to share motion.
 *
 * `coverage` is the fraction of the destination's detected humanoid roles that found a source
 * joint. `compatible` is a threshold on that coverage AND on the presence of the Hips role on both
 * sides, because without a common root there is no meaningful rest-pose alignment. Both numbers are
 * reported to the user with the list of roles that did not map, so a failed retarget explains
 * itself instead of producing a silently broken pose.
 */
struct RetargetMap {
    std::vector<int> dstToSrc;             /* per destination joint: source joint, or -1 */
    /* Per destination joint: 1 / (number of destination joints carrying that joint's role). A role
       the destination expresses with three spine joints receives a third of the source's rotation
       each, so the total bend is preserved instead of being applied three times over. Always 1 for
       the common one-joint-per-role case, and 0 for an unmapped joint. */
    std::vector<float> shareFactor;
    std::vector<int> srcRoleToJoint;       /* per Role: source joint, or -1 */
    std::vector<int> dstRoleToJoint;       /* per Role: destination joint, or -1 */
    float scale = 1.0f;                    /* destination hip height / source hip height */
    float coverage = 0.0f;                 /* 0..1 */
    bool compatible = false;
    std::vector<std::string> missingRoles; /* roles absent from either side */
    std::vector<std::string> notes;        /* what the mapper decided, for the diagnostics panel */

    int mappedJoints() const;
};

/** Builds the map. Never fails on a mismatch; it reports the mismatch through `coverage`,
 *  `compatible` and `missingRoles` so the caller can refuse the operation with a reason. */
bool buildRetargetMap(const Skeleton& src, const Skeleton& dst, RetargetMap* out);

/**
 * Applies `map` to `in` (a pose in source skeleton space) producing a destination-space pose in
 * `out`.
 *
 * Rotation retargeting is rest-relative: for each mapped joint the source's deviation from its OWN
 * rest rotation is re-expressed on the destination's rest rotation,
 *     out.r = dstRestR * (srcRestR^-1 * in.r)
 * so a T-pose source maps to a T-pose destination and an A-pose source maps to an A-pose
 * destination -- which is exactly the rest-pose alignment problem, and why retargeting Fox (whose
 * legs are built for a quadruped) onto a humanoid does not simply paste rotations across.
 *
 * Translation is scaled by the hip-height ratio for the Hips role only; every other joint keeps the
 * destination's own rest translation, because source bone lengths are meaningless on a differently
 * proportioned skeleton. Scale channels are not transferred at all.
 */
void retargetPose(const Skeleton& src, const Skeleton& dst, const RetargetMap& map,
                  const Pose& in, Pose* out);

/* ================================================================================ Player ======= */

/** Transport/playback state reported to the UI. Every field is a real value, not a placeholder. */
struct PlayerState {
    int clip = -1;
    std::string clipName;
    float position = 0.0f;
    float duration = 0.0f;
    float speed = 1.0f;
    bool playing = false;
    bool looping = true;
    bool transitioning = false;
    int previousClip = -1;
    /* The fade source's OWN clock, which runs independently of `position` during a crossfade. A
       renderer that maps this onto Filament's Animator.applyCrossFade(previousIndex, previousTime,
       alpha) needs all three; deriving previousTime from position and the progress would be wrong as
       soon as the two clips have different durations. */
    float previousPosition = 0.0f;
    float transitionProgress = 1.0f;
    int layerCount = 0;
    int morphCount = 0;
    int jointCount = 0;
};

/**
 * One blended frame, expressed the way a GPU renderer's crossfade primitive wants it.
 *
 * This exists because the alpha convention is easy to get backwards, and getting it backwards does not
 * crash anything -- it silently plays every transition in reverse, fading OUT of the clip you asked for.
 * Filament documents its own convention explicitly (Animator.applyCrossFade, gltfio): the animation
 * passed to applyAnimation is the CURRENT one and corresponds to alpha = 1, while the previous
 * animation passed to applyCrossFade corresponds to alpha = 0. So `alpha` here is the weight of
 * `currentIndex`, and the renderer applies:
 *
 *     applyAnimation(currentIndex, currentTime);
 *     if (blending) applyCrossFade(previousIndex, previousTime, alpha);
 *     updateBoneMatrices();
 *
 * `PlayerState::transitionProgress` is deliberately NOT this value: progress runs 0 -> 1 over the fade
 * and is about how far along the transition is, whereas alpha is a blend weight. They coincide only
 * because both happen to be measured toward the incoming clip; naming them separately keeps a future
 * easing curve from silently changing what a renderer blends.
 */
struct CrossFadeCommand {
    int currentIndex = -1;      /* the clip fading IN; the renderer samples it first */
    float currentTime = 0.0f;
    int previousIndex = -1;     /* the clip fading OUT; -1 when nothing is blending */
    float previousTime = 0.0f;  /* the outgoing clip's OWN clock, not derived from currentTime */
    float alpha = 1.0f;         /* weight of currentIndex: 1 = current only, 0 = previous only */
    bool blending = false;      /* false means apply currentIndex alone */

    /** Builds the command from a player's state. */
    static CrossFadeCommand from(const PlayerState& s) {
        CrossFadeCommand c;
        c.currentIndex = s.clip;
        c.currentTime = s.position;
        c.blending = s.transitioning && s.previousClip >= 0;
        if (c.blending) {
            c.previousIndex = s.previousClip;
            c.previousTime = s.previousPosition;
            c.alpha = s.transitionProgress;
        } else {
            c.previousIndex = -1;
            c.previousTime = 0.0f;
            c.alpha = 1.0f;
        }
        return c;
    }
};

/** A layer: an extra clip blended over the base pose, optionally additive, optionally masked. */
struct Layer {
    int clip = -1;
    float weight = 0.0f;
    float position = 0.0f;
    float speed = 1.0f;
    bool additive = false;
    bool enabled = false;
    BoneMask mask;
    Pose pose;
    SampleCache cache;
};

/**
 * The animation player. One instance owns one skeleton and a set of clips, and produces one Pose
 * per frame.
 *
 * Loading: loadGlb() takes an already-imported af::GlbAsset (so the existing hardened importer is
 * reused rather than duplicated) and builds the skeleton from its first skin and one clip per
 * imported animation. loadAuthored() builds the skeleton from the engine's generated rig and takes
 * the 16 authored clips from af::builtinClips(). Both paths can be combined: addClip() accepts
 * further clips, and retarget() lets a clip authored for one skeleton play on another.
 *
 * Per frame: advance(dt) moves every clock, fires events, extracts root motion, samples the base
 * clip (or the crossfade pair), applies each enabled layer, then applies morph tracks and the face
 * controller. The result is in currentPose().
 */
class Player {
public:
    Player();
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    /* ---- loading ---- */
    bool loadGlb(const GlbAsset& asset, std::string* error);
    bool loadAuthored(const Rig& rig, std::string* error);
    /* For an imported file that carries a skinnable skeleton but NO animation of its own: build the
       skeleton from the file's skin and bind the engine's authored library to it by role. This is the
       difference between a statue and a moving figure for the very common export that writes a rig and
       no clips. Refuses (returns false, with the reason in `error`) when the file has no skin, no
       inverse bind matrices, or no role the library can bind to - it never invents motion. */
    bool loadAuthoredForGlb(const GlbAsset& asset, std::string* error);
    int addClip(Clip clip);
    void clear();

    /* ---- introspection ---- */
    const Skeleton& skeleton() const { return skeleton_; }
    const std::vector<Clip>& clips() const { return clips_; }
    int clipCount() const { return static_cast<int>(clips_.size()); }
    int clipIndex(const std::string& name) const;
    const Clip* clipAt(int index) const;
    int morphCount() const { return morphCount_; }
    const std::vector<std::string>& morphNames() const { return morphNames_; }
    FaceController& face() { return face_; }
    const FaceController& face() const { return face_; }
    StateMachine& stateMachine() { return machine_; }
    const StateMachine& stateMachine() const { return machine_; }
    RootMotion& rootMotion() { return rootMotion_; }
    const std::vector<std::string>& loadNotes() const { return loadNotes_; }

    /* ---- transport ---- */
    bool play(int clipIndex);
    bool play(const std::string& clipName);
    void pause();
    void resume();
    void stop();
    /* Drops the clip selection and returns the pose to the skeleton's rest (bind) pose, recomputing its
       matrices - the equivalent of gltfio's Animator.resetBoneMatrices(), and what a viewer shows before
       anything is played. Playback stops; the clip list is untouched, so play() still starts any clip
       afterwards. loadGlb() auto-selects clip 0 and marks it playing, which suits a player about to be
       ticked but not a model that has merely been loaded: without this, "show me the file" and "play its
       first animation from frame 0" are the same request and cannot be told apart.
       While nothing is selected seek() is a no-op, so callers must check currentClip() rather than
       assume a seek moved something. */
    void resetToBindPose();
    void seek(float seconds);
    void setSpeed(float speed);
    void setLoop(bool loop);
    bool isPlaying() const { return playing_; }
    float position() const { return position_; }
    /** The crossfade source's own clock. Equals 0 when no transition is running. */
    float previousPosition() const { return previousPosition_; }
    /** This frame expressed for a renderer's crossfade primitive. See CrossFadeCommand for the alpha
        convention, which is the one Filament's Animator.applyCrossFade documents. */
    CrossFadeCommand crossFadeCommand() const { return CrossFadeCommand::from(state()); }
    float speed() const { return speed_; }
    bool looping() const { return looping_; }
    int currentClip() const { return currentClip_; }
    float currentDuration() const;
    /** Starts a crossfade to `clipIndex` over `seconds`. Returns false when the clip is unknown. */
    bool crossfadeTo(int clipIndex, float seconds);
    bool crossfadeTo(const std::string& clipName, float seconds);
    void setCrossfadeEnabled(bool enabled) { crossfadeEnabled_ = enabled; }
    bool crossfadeEnabled() const { return crossfadeEnabled_; }

    /* ---- state machine ---- */
    /**
     * Builds one state per loaded clip, named after the clip, with a transition between every
     * ordered pair at `crossfadeSeconds`, and arms the machine to drive playback. Returns false when
     * no clip is loaded -- a machine with no states cannot drive anything and is not pretended to.
     */
    bool buildStateMachine(float crossfadeSeconds);
    void setStateMachineDriven(bool driven);
    bool stateMachineDriven() const { return machineDriven_; }
    /** Arms the machine (if it has states) and requests a transition. Returns false when the state
     *  is unknown or no transition edge exists, leaving playback untouched. */
    bool requestState(const std::string& name);

    /* ---- lip-sync ---- */
    LipSyncTrack& lipSync() { return lipTrack_; }
    const LipSyncTrack& lipSync() const { return lipTrack_; }
    /** Prepares an approximate text lip-sync timeline spread evenly over `totalSeconds` and enables
     *  viseme driving. `totalSeconds` is the caller's own estimate (for Android TextToSpeech, whose
     *  per-phoneme timing is not observable); pass <= 0 to use the default syllable rate instead. */
    void speak(const std::string& text, float totalSeconds);
    void stopSpeaking();
    bool isSpeaking() const { return speaking_; }
    float lipSyncPosition() const { return lipPosition_; }

    /* ---- layers ---- */
    int addLayer(int clipIndex, float weight, bool additive);
    int layerCount() const { return static_cast<int>(layers_.size()); }
    Layer* layerAt(int index);
    bool setLayerClip(int index, int clipIndex);
    bool setLayerWeight(int index, float weight);
    bool setLayerEnabled(int index, bool enabled);
    bool setLayerMask(int index, const BoneMask& mask);
    bool setLayerAdditive(int index, bool additive);

    /* ---- events ---- */
    using EventListener = std::function<void(const std::string& name, float value, float time)>;
    void setEventListener(EventListener listener) { eventListener_ = std::move(listener); }
    /** Adds an authored event to a clip. */
    bool addEvent(int clipIndex, float time, const std::string& name, float value);

    /* ---- retargeting ---- */
    /** Retargets clip `srcClip` from `srcSkeleton` onto this player's skeleton, appending the
     *  result as a new clip. Returns the new clip index, or -1 with `error` set. */
    int retarget(const Skeleton& srcSkeleton, const Clip& srcClip, std::string* error);
    /** Reports compatibility of `other` against this player's skeleton without modifying anything. */
    bool checkCompatibility(const Skeleton& other, RetargetMap* outReport) const;

    /* ---- per-frame ---- */
    /** Advances every clock by `dt` seconds and evaluates the blended pose. Safe with dt == 0
     *  (re-evaluates the current time), which is what scrubbing uses. */
    void advance(float dt);
    const Pose& currentPose() const { return currentPose_; }
    Pose& currentPose() { return currentPose_; }
    PlayerState state() const;

    /* ---- performance ---- */
    struct Stats {
        double lastAdvanceMicros = 0.0;
        double avgAdvanceMicros = 0.0;
        double maxAdvanceMicros = 0.0;
        uint64_t frames = 0;
        uint64_t posesSampled = 0;
        int jointCount = 0;
        int channelCount = 0;
    };
    const Stats& stats() const { return stats_; }
    void resetStats();

private:
    void ensureBuffers();
    void applyClipToPose(const Clip& clip, float time, SampleCache* cache, Pose* out);
    float wrapTime(float t, float duration) const;
    void fireEvents(const Clip& clip, float from, float to);

    Skeleton skeleton_;
    std::vector<Clip> clips_;
    std::vector<std::string> morphNames_;
    std::vector<std::string> loadNotes_;
    int morphCount_ = 0;

    FaceController face_;
    StateMachine machine_;
    RootMotion rootMotion_;

    int currentClip_ = -1;
    int previousClip_ = -1;
    float position_ = 0.0f;
    float previousPosition_ = 0.0f;
    float transitionTime_ = -1.0f;
    float transitionDuration_ = 0.0f;
    float speed_ = 1.0f;
    bool playing_ = false;
    bool looping_ = true;
    bool crossfadeEnabled_ = true;

    Pose currentPose_;
    Pose scratchA_;
    Pose scratchB_;
    SampleCache cacheA_;
    SampleCache cacheB_;
    std::vector<Layer> layers_;
    EventListener eventListener_;
    Stats stats_;
    bool buffersReady_ = false;
    bool machineDriven_ = false;
    LipSyncTrack lipTrack_;
    float lipPosition_ = 0.0f;
    bool speaking_ = false;
};

/* ================================================================= library / catalogue ========= */

/**
 * The searchable animation library: every clip the app can offer, with its category, duration,
 * source and licence. Built from the clips a Player loaded plus the authored engine library, so
 * the catalogue can never list a clip that no loaded asset actually provides.
 */
struct LibraryEntry {
    std::string name;
    Category category = Category::Uncategorized;
    float duration = 0.0f;
    bool looping = true;
    bool authored = false;
    std::string source;         /* "Fox.glb", "AvatarForge authored library", ... */
    std::string licence;        /* attribution carried through to THIRD_PARTY_NOTICES.md */
    int clipIndex = -1;         /* index in the owning player, or -1 for a not-yet-loaded entry */
};

class AnimationLibrary {
public:
    void clear();
    void addEntry(LibraryEntry entry);
    /** Appends every clip of `player`, tagged with `source` and `licence`. */
    void addFromPlayer(const Player& player, const std::string& source, const std::string& licence);
    /** Appends the engine's 16 authored clips (af::builtinClips()), which need no asset. */
    void addAuthoredLibrary();

    const std::vector<LibraryEntry>& entries() const { return entries_; }
    int count() const { return static_cast<int>(entries_.size()); }
    /** Case-insensitive substring match on name, and exact match on category. */
    std::vector<int> search(const std::string& query, Category category = Category::COUNT) const;
    std::vector<int> byCategory(Category c) const;
    const LibraryEntry* at(int index) const;
    int indexOf(const std::string& name) const;
    /** Counts per category, for the library UI's headers. */
    std::vector<int> categoryHistogram() const;

private:
    std::vector<LibraryEntry> entries_;
};

} /* namespace anim */
} /* namespace af */

#endif /* AF_ANIM_H */
