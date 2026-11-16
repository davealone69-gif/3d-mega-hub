/*
 * af_anim_statemachine.cpp -- clip transitions with crossfades, and root-motion extraction.
 *
 * The state machine owns TIME, not poses. advance() moves the current clip's clock, the outgoing
 * clip's clock, and the crossfade alpha; output() reports which two clips to sample and how to
 * weight them. Deliberately nothing here touches a Pose, which is what lets the same machine drive
 * both backends:
 *
 *   AvatarForge GLES   Player samples both clips, blendPoses() merges them, the CPU deforms or the
 *                      bone palette goes to the skinning shader;
 *   Filament           the two clips are the asset's own glTF animations, so gltfio samples them and
 *                      Animator::applyCrossFade(previousIndex, previousTime, alpha) does the merge.
 *                      output() maps onto that call one-to-one.
 *
 * A transition that has no declared edge is REFUSED: requestState() returns false and changes
 * nothing. Snapping to the requested state would look like the machine works while hiding that the
 * graph is missing the edge the caller expected.
 */
#include "af_anim.h"

#include <algorithm>
#include <cmath>

namespace af {
namespace anim {

/* =========================================================================== StateMachine ====== */

int StateMachine::addState(const std::string& name, int clipIndex, bool looping) {
    if (name.empty() || clipIndex < 0) return -1;
    if (stateIndex(name) >= 0) return -1;              /* a duplicate name would make edges ambiguous */
    State s;
    s.name = name;
    s.clip = clipIndex;
    s.looping = looping;
    states_.push_back(s);
    if (current_ < 0) {
        current_ = static_cast<int>(states_.size()) - 1;
        currentTime_ = 0.0f;
    }
    return static_cast<int>(states_.size()) - 1;
}

bool StateMachine::addTransition(const std::string& from, const std::string& to, float crossfadeSeconds) {
    const int a = stateIndex(from);
    const int b = stateIndex(to);
    if (a < 0 || b < 0) return false;
    Edge e;
    e.from = a;
    e.to = b;
    e.crossfade = std::isfinite(crossfadeSeconds) ? std::max(0.0f, crossfadeSeconds) : 0.0f;
    edges_.push_back(e);
    return true;
}

void StateMachine::addAllTransitions(float crossfadeSeconds) {
    for (size_t a = 0; a < states_.size(); ++a) {
        for (size_t b = 0; b < states_.size(); ++b) {
            if (a == b) continue;
            addTransition(states_[a].name, states_[b].name, crossfadeSeconds);
        }
    }
}

int StateMachine::stateIndex(const std::string& name) const {
    for (size_t i = 0; i < states_.size(); ++i) {
        if (states_[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

const std::string& StateMachine::stateName(int state) const {
    static const std::string kEmpty;
    if (state < 0 || state >= static_cast<int>(states_.size())) return kEmpty;
    return states_[static_cast<size_t>(state)].name;
}

int StateMachine::clipForState(int state) const {
    if (state < 0 || state >= static_cast<int>(states_.size())) return -1;
    return states_[static_cast<size_t>(state)].clip;
}

bool StateMachine::transitionExists(int from, int to, float* outCrossfade) const {
    for (const Edge& e : edges_) {
        if (e.from != from || e.to != to) continue;
        if (outCrossfade) *outCrossfade = e.crossfade;
        return true;
    }
    return false;
}

bool StateMachine::requestState(int state) {
    if (state < 0 || state >= static_cast<int>(states_.size())) return false;
    if (state == current_) return true;                 /* already there: a no-op, not an error */
    float crossfade = 0.0f;
    if (current_ >= 0 && !transitionExists(current_, state, &crossfade)) return false;
    previous_ = current_;
    previousTime_ = currentTime_;
    current_ = state;
    currentTime_ = 0.0f;
    transitionDuration_ = crossfade;
    transitionTime_ = (crossfade > 0.0f) ? 0.0f : -1.0f;
    return true;
}

bool StateMachine::requestState(const std::string& name) {
    return requestState(stateIndex(name));
}

void StateMachine::reset(int state) {
    if (state < 0 || state >= static_cast<int>(states_.size())) return;
    current_ = state;
    previous_ = -1;
    currentTime_ = 0.0f;
    previousTime_ = 0.0f;
    transitionTime_ = -1.0f;
    transitionDuration_ = 0.0f;
}

namespace {

/** Advances a clip clock, honouring the clip's own loop flag and duration. A zero or non-finite
 *  duration pins the clock at 0 rather than dividing by it. */
float advanceClock(float t, float dt, const Clip* clip) {
    if (!clip) return t + dt;
    const float duration = clip->duration;
    if (!std::isfinite(duration) || duration <= 0.0f) return 0.0f;
    float next = t + dt;
    if (!std::isfinite(next)) return 0.0f;
    if (clip->looping) {
        next = std::fmod(next, duration);
        if (next < 0.0f) next += duration;
        return next;
    }
    return clampf(next, 0.0f, duration);
}

} /* namespace */

void StateMachine::advance(float dt, const std::vector<Clip>& clips) {
    if (!std::isfinite(dt) || dt < 0.0f) dt = 0.0f;
    const Clip* currentClip = nullptr;
    const Clip* previousClip = nullptr;
    if (current_ >= 0 && current_ < static_cast<int>(states_.size())) {
        const int ci = states_[static_cast<size_t>(current_)].clip;
        if (ci >= 0 && ci < static_cast<int>(clips.size())) currentClip = &clips[static_cast<size_t>(ci)];
    }
    if (previous_ >= 0 && previous_ < static_cast<int>(states_.size())) {
        const int pi = states_[static_cast<size_t>(previous_)].clip;
        if (pi >= 0 && pi < static_cast<int>(clips.size())) previousClip = &clips[static_cast<size_t>(pi)];
    }
    currentTime_ = advanceClock(currentTime_, dt, currentClip);
    if (transitionTime_ >= 0.0f) {
        previousTime_ = advanceClock(previousTime_, dt, previousClip);
        transitionTime_ += dt;
        if (transitionDuration_ <= 0.0f || transitionTime_ >= transitionDuration_) {
            transitionTime_ = -1.0f;
            previous_ = -1;
            previousTime_ = 0.0f;
            transitionDuration_ = 0.0f;
        }
    }
}

const std::string& StateMachine::currentStateName() const { return stateName(current_); }

float StateMachine::transitionProgress() const {
    if (transitionTime_ < 0.0f || transitionDuration_ <= 0.0f) return 1.0f;
    return saturate(transitionTime_ / transitionDuration_);
}

StateMachine::Output StateMachine::output() const {
    Output o;
    o.clipA = (current_ >= 0 && current_ < static_cast<int>(states_.size()))
                  ? states_[static_cast<size_t>(current_)].clip : -1;
    o.timeA = currentTime_;
    if (transitionTime_ >= 0.0f && previous_ >= 0 && previous_ < static_cast<int>(states_.size())) {
        o.clipB = states_[static_cast<size_t>(previous_)].clip;
        o.timeB = previousTime_;
        o.alpha = transitionProgress();
        o.transitioning = true;
    } else {
        o.clipB = -1;
        o.timeB = 0.0f;
        o.alpha = 1.0f;
        o.transitioning = false;
    }
    return o;
}

/* ============================================================================== RootMotion ===== */

Vec3 RootMotion::consume() {
    const Vec3 out = accumulated;
    accumulated = Vec3(0, 0, 0);
    return out;
}

void RootMotion::reset() { accumulated = Vec3(0, 0, 0); }

int rootMotionJoint(const Skeleton& sk, const Clip& clip) {
    /* Depth of a joint = how many ancestors it has. Computed inline and bounded so a corrupt parent
       array cannot spin: the walk stops at 64 or at the first repeat of the root. */
    auto depthOf = [&](int j) {
        int depth = 0;
        int cur = (j >= 0 && j < sk.count()) ? sk.parents[static_cast<size_t>(j)] : -1;
        while (cur >= 0 && cur < sk.count() && depth < 64) {
            depth += 1;
            cur = sk.parents[static_cast<size_t>(cur)];
        }
        return depth;
    };

    int hipsJoint = -1, hipsDepth = -1;
    int shallowest = -1, shallowestDepth = 1 << 30;
    for (const Channel& ch : clip.channels) {
        if (ch.path != Path::Translation) continue;
        if (ch.joint < 0 || ch.joint >= sk.count()) continue;
        const int depth = depthOf(ch.joint);
        if (depth < shallowestDepth) { shallowestDepth = depth; shallowest = ch.joint; }
        /* Deepest Hips-chain joint: a rig that translates both a container node and the hips should
           be measured at the hips, because that is the bone the skin follows. */
        if (sk.roles.size() == static_cast<size_t>(sk.count()) &&
            sk.roles[static_cast<size_t>(ch.joint)] == Role::Hips && depth > hipsDepth) {
            hipsDepth = depth;
            hipsJoint = ch.joint;
        }
    }
    if (hipsJoint >= 0) return hipsJoint;
    return shallowest;   /* -1 when the clip translates nothing: no root motion, and none invented */
}

Vec3 rootTranslationDelta(const Skeleton& sk, const Clip& clip, float fromTime, float toTime,
                          SampleCache* cache) {
    (void)cache;   /* the chain walk below re-reads at most a handful of channels; a cursor keyed to
                      one channel would not help and would make the signature lie about what it uses */
    const int rootJoint = rootMotionJoint(sk, clip);
    if (rootJoint < 0 || rootJoint >= sk.count()) return Vec3(0, 0, 0);

    /* Only this joint's ancestor chain affects its world position, so only that chain is walked. A
       fixed array keeps the function allocation-free; 128 is far past any real skeleton depth (the
       engine's own 75-joint rig is 10 deep) and the walk stops rather than overflowing. */
    int chain[128];
    int chainLen = 0;
    int cur = rootJoint;
    int guard = 0;
    while (cur >= 0 && cur < sk.count() && chainLen < 128 && guard++ <= sk.count()) {
        chain[chainLen++] = cur;
        cur = sk.parents[static_cast<size_t>(cur)];
    }

    /* Local TRS of one chain joint at one time: the clip's channels where it has them, the skeleton's
       rest transform where it does not. Scale is never animated on a root chain in practice and is
       taken from the rest pose, which keeps this to the two channels that move the root. */
    auto localAt = [&](int j, float t, Vec3* outT, Quat* outR) {
        const size_t k = static_cast<size_t>(j);
        *outT = sk.restT[k];
        *outR = sk.restR[k];
        for (const Channel& ch : clip.channels) {
            if (ch.joint != j) continue;
            Vec4 v;
            if (ch.path == Path::Translation) {
                sampleChannelVec(ch, t, nullptr, &v);
                *outT = Vec3(v.x, v.y, v.z);
            } else if (ch.path == Path::Rotation) {
                sampleChannelVec(ch, t, nullptr, &v);
                const Quat q(v.x, v.y, v.z, v.w);
                if (std::fabs(q.dot(q)) > 1e-8f) *outR = q.normalized();
            }
        }
    };
    auto worldPositionAt = [&](float t) {
        Mat4 world = Mat4::identity();
        for (int i = chainLen - 1; i >= 0; --i) {
            Vec3 lt; Quat lr;
            localAt(chain[i], t, &lt, &lr);
            world = world * Mat4::trs(lt, lr, sk.restS[static_cast<size_t>(chain[i])]);
        }
        return world.transformPoint(Vec3(0, 0, 0));
    };
    return worldPositionAt(toTime) - worldPositionAt(fromTime);
}

} /* namespace anim */
} /* namespace af */
