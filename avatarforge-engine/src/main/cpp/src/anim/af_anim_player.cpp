/*
 * af_anim_player.cpp -- the player: transport, crossfades, layers, events, root motion, morph/facial
 *                       merge, retargeted-clip baking and frame statistics.
 *
 * ONE POSE PER FRAME, WHATEVER THE COMBINATION
 * --------------------------------------------
 * advance(dt) is the only per-frame entry point and it always produces exactly one Pose, composed
 * in this order:
 *
 *   1. base       the current clip, or a weighted blend of the outgoing and incoming clip while a
 *                 crossfade runs (and, when the state machine is driving, the two clips and the
 *                 alpha come from StateMachine::output() instead of the Player's own transport);
 *   2. layers     each enabled layer blended over that, masked (upper/lower body) or additive;
 *   3. morphs     the clip's own weights channels, merged with the live FaceController by max() so
 *                 an interactive expression or viseme never gets erased by an authored morph track;
 *   4. matrices   local -> world -> skinning palette.
 *
 * Order matters and is deliberate: a layer must be able to override the base, and the face must be
 * able to override both, because that is the direction the user's input flows.
 *
 * WHAT IS MEASURED
 * ----------------
 * Stats records the wall time of advance() itself, not of the frame that contains it, so the number
 * is the cost of this module and can be compared against a frame budget directly. The host test
 * suite reports it; no frame rate is claimed anywhere in this file, because this file cannot see a
 * display.
 */
#include "af_anim.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <set>

namespace af {
namespace anim {

namespace {

double nowMicros() {
    return std::chrono::duration<double, std::micro>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/** Saturated, finite, non-negative delta. A negative or NaN dt from a stalled clock would otherwise
 *  run the timeline backwards or poison every keyframe comparison downstream. */
float sanitiseDt(float dt) {
    if (!std::isfinite(dt) || dt < 0.0f) return 0.0f;
    /* A 2-second cap: returning from a background pause hands the player an enormous dt, and
       advancing a walk cycle by two seconds in one step is a visible teleport, not a catch-up. */
    return dt > 2.0f ? 2.0f : dt;
}

} /* namespace */

Player::Player() = default;
Player::~Player() = default;

/* ================================================================================== loading ==== */

void Player::clear() {
    skeleton_ = Skeleton();
    clips_.clear();
    morphNames_.clear();
    loadNotes_.clear();
    morphCount_ = 0;
    face_.clear();
    machine_ = StateMachine();
    machineDriven_ = false;
    rootMotion_.reset();
    currentClip_ = -1;
    previousClip_ = -1;
    position_ = 0.0f;
    previousPosition_ = 0.0f;
    transitionTime_ = -1.0f;
    transitionDuration_ = 0.0f;
    playing_ = false;
    layers_.clear();
    buffersReady_ = false;
    lipTrack_.clear();
    lipPosition_ = 0.0f;
    speaking_ = false;
    currentPose_ = Pose();
    scratchA_ = Pose();
    scratchB_ = Pose();
}

void Player::ensureBuffers() {
    if (buffersReady_) return;
    currentPose_.allocate(skeleton_, morphCount_);
    scratchA_.allocate(skeleton_, morphCount_);
    scratchB_.allocate(skeleton_, morphCount_);
    for (Layer& l : layers_) l.pose.allocate(skeleton_, morphCount_);
    buffersReady_ = true;
}

bool Player::loadGlb(const GlbAsset& asset, std::string* error) {
    auto fail = [&](const std::string& msg) { if (error) *error = msg; return false; };
    clear();

    std::string skelError;
    if (!asset.skins.empty()) {
        if (!Skeleton::fromGlb(asset, 0, &skeleton_, &skelError)) return fail(skelError);
        if (asset.skins.size() > 1) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "the file declares %zu skins; the first is used and the others are not played",
                          asset.skins.size());
            loadNotes_.push_back(buf);
        }
    } else {
        /* No skeleton at all. A morph-only asset is still playable, and refusing it would mean an
           AnimatedMorphCube import reports "no animation" while its weights channel sits right
           there in the file. */
        if (!Skeleton::fromGlbNodes(asset, &skeleton_, &skelError)) return fail(skelError);
        loadNotes_.push_back("the file has no skin: nothing can be skinned or retargeted, but its "
                             "node transforms and morph-target animation still play");
    }

    morphCount_ = totalMorphTargetsForAsset(asset);
    for (const ImportedMesh& m : asset.meshes) {
        for (int i = 0; i < m.morphTargetCount; ++i) {
            morphNames_.push_back(i < static_cast<int>(m.targetNames.size()) ? m.targetNames[static_cast<size_t>(i)]
                                                                            : std::string());
        }
    }
    face_.configure(morphCount_, morphNames_);
    if (morphCount_ > 0 && !face_.hasNamedTargets()) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "%d morph targets were imported but the file declares no targetNames, so they "
                      "are addressable by index only",
                      morphCount_);
        loadNotes_.push_back(buf);
    }

    ensureBuffers();

    std::vector<std::string> skipped;
    int loaded = 0;
    for (int a = 0; a < static_cast<int>(asset.animations.size()); ++a) {
        Clip clip;
        std::string clipError;
        if (!Clip::fromGlb(asset, a, skeleton_, &clip, &skipped, &clipError)) {
            loadNotes_.push_back("animation " + std::to_string(a) + " was not playable: " + clipError);
            continue;
        }
        clips_.push_back(std::move(clip));
        ++loaded;
    }
    for (const std::string& s : skipped) loadNotes_.push_back(s);

    if (loaded == 0 && morphCount_ == 0) {
        return fail("the file contains no animation this skeleton can play");
    }
    if (loaded > 0) {
        currentClip_ = 0;
        position_ = 0.0f;
        playing_ = true;
        looping_ = clips_[0].looping;
        cacheA_.bind(clips_[0]);
        cacheB_.bind(clips_[0]);
    }
    char buf[192];
    std::snprintf(buf, sizeof(buf), "imported %d joint(s), %d clip(s), %d morph target(s)",
                  skeleton_.skinSlotCount(), loaded, morphCount_);
    loadNotes_.push_back(buf);
    return true;
}

bool Player::loadAuthoredForGlb(const GlbAsset& asset, std::string* error) {
    auto fail = [&](const std::string& msg) { if (error) *error = msg; return false; }
    ;
    clear();

    if (asset.skins.empty()) {
        return fail("the file has no skin, so there is no skeleton for the authored library to drive");
    }
    std::string skelError;
    if (!Skeleton::fromGlb(asset, 0, &skeleton_, &skelError)) return fail(skelError);
    if (!skeleton_.hasInverseBind) {
        return fail("the file's skin carries no inverse bind matrices, so nothing can be skinned");
    }
    /* Roles are the whole point of this path: the authored tracks are placed by Role, so a skeleton
       that resolves few roles binds few tracks. assignRoles() is the same detector the rest of the core
       uses, and the coverage is reported below rather than left implicit. */
    const int roles = skeleton_.assignRoles();

    morphCount_ = totalMorphTargetsForAsset(asset);
    for (const ImportedMesh& m : asset.meshes) {
        for (int i = 0; i < m.morphTargetCount; ++i) {
            morphNames_.push_back(i < static_cast<int>(m.targetNames.size())
                                      ? m.targetNames[static_cast<size_t>(i)] : std::string());
        }
    }
    face_.configure(morphCount_, morphNames_);
    ensureBuffers();

    const std::vector<af::Clip>& library = af::builtinClips();
    std::vector<std::string> skipped;
    int loaded = 0;
    for (const af::Clip& authored : library) {
        Clip clip;
        const size_t skipsBefore = skipped.size();
        if (!Clip::fromAuthoredByRole(authored, skeleton_, &clip, &skipped)) {
            /* fromAuthoredByRole reports WHY it refused (a calibration pose, an absent role, an
               uncovered track name). Only add the generic note when it stayed silent: claiming a clip
               "bound to no joint" next to a note saying it was refused on purpose is a contradiction,
               and the specific reason is the one a caller can act on. */
            if (skipped.size() == skipsBefore) {
                loadNotes_.push_back("authored clip '" + authored.name +
                                     "' bound to no joint of this file's skeleton");
            }
            continue;
        }
        clips_.push_back(std::move(clip));
        ++loaded;
    }
    for (const std::string& s2 : skipped) loadNotes_.push_back(s2);
    if (loaded == 0) {
        return fail("no authored clip could be bound to this file's skeleton by role");
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "the file carries no animation of its own; bound %d of %d engine-authored clip(s) to "
                  "its %d-joint skeleton by humanoid role (%d of %d roles present)",
                  loaded, static_cast<int>(library.size()), skeleton_.count(), roles,
                  static_cast<int>(Role::COUNT));
    loadNotes_.push_back(buf);
    /* Land in the bind pose, exactly as the engine expects after loadGlb(): loading a file is not the
       same request as playing its first animation. */
    resetToBindPose();
    return true;
}

bool Player::loadAuthored(const Rig& rig, std::string* error) {
    auto fail = [&](const std::string& msg) { if (error) *error = msg; return false; };
    clear();
    std::string skelError;
    if (!Skeleton::fromRig(rig, &skeleton_, &skelError)) return fail(skelError);

    /* The generated avatar's face is the engine's own 52 ARKit channels, baked by
       af_engine_bake_blendshapes in exactly this order. */
    morphCount_ = kBlendshapeCount;
    morphNames_.assign(kBlendshapeNames, kBlendshapeNames + kBlendshapeCount);
    face_.configure(morphCount_, morphNames_);
    ensureBuffers();

    const std::vector<af::Clip>& library = af::builtinClips();
    std::vector<std::string> skipped;
    for (const af::Clip& authored : library) {
        Clip clip;
        if (!Clip::fromAuthored(authored, skeleton_, &clip, &skipped)) {
            loadNotes_.push_back("authored clip '" + authored.name + "' bound to no joint of this rig");
            continue;
        }
        clips_.push_back(std::move(clip));
    }
    for (const std::string& s : skipped) loadNotes_.push_back(s);
    if (clips_.empty()) return fail("no authored clip could be bound to this rig");

    currentClip_ = 0;
    position_ = 0.0f;
    playing_ = true;
    looping_ = clips_[0].looping;
    cacheA_.bind(clips_[0]);
    cacheB_.bind(clips_[0]);
    char buf[192];
    std::snprintf(buf, sizeof(buf), "bound %d authored clip(s) to %d rig joint(s) and %d ARKit channels",
                  static_cast<int>(clips_.size()), skeleton_.count(), morphCount_);
    loadNotes_.push_back(buf);
    return true;
}

int Player::addClip(Clip clip) {
    clips_.push_back(std::move(clip));
    if (currentClip_ < 0 && !clips_.empty()) {
        currentClip_ = static_cast<int>(clips_.size()) - 1;
        cacheA_.bind(clips_[static_cast<size_t>(currentClip_)]);
        cacheB_.bind(clips_[static_cast<size_t>(currentClip_)]);
        looping_ = clips_[static_cast<size_t>(currentClip_)].looping;
    }
    return static_cast<int>(clips_.size()) - 1;
}

int Player::clipIndex(const std::string& name) const {
    for (size_t i = 0; i < clips_.size(); ++i) if (clips_[i].name == name) return static_cast<int>(i);
    return -1;
}

const Clip* Player::clipAt(int index) const {
    if (index < 0 || index >= static_cast<int>(clips_.size())) return nullptr;
    return &clips_[static_cast<size_t>(index)];
}

/* ================================================================================ transport ==== */

float Player::currentDuration() const {
    const Clip* c = clipAt(currentClip_);
    return c ? c->duration : 0.0f;
}

float Player::wrapTime(float t, float duration) const {
    if (!(duration > 0.0f)) return 0.0f;
    if (!std::isfinite(t)) return 0.0f;
    if (looping_) {
        const float w = std::fmod(t, duration);
        return w < 0.0f ? w + duration : w;
    }
    return clampf(t, 0.0f, duration);
}

bool Player::play(int clipIndex) {
    if (clipIndex < 0 || clipIndex >= static_cast<int>(clips_.size())) return false;
    ensureBuffers();
    previousClip_ = currentClip_;
    previousPosition_ = position_;
    currentClip_ = clipIndex;
    position_ = 0.0f;
    transitionTime_ = -1.0f;
    transitionDuration_ = 0.0f;
    playing_ = true;
    looping_ = clips_[static_cast<size_t>(clipIndex)].looping;
    cacheA_.bind(clips_[static_cast<size_t>(clipIndex)]);
    if (previousClip_ >= 0 && previousClip_ < static_cast<int>(clips_.size())) {
        cacheB_.bind(clips_[static_cast<size_t>(previousClip_)]);
    }
    return true;
}

bool Player::play(const std::string& clipName) { return play(clipIndex(clipName)); }

void Player::pause() { playing_ = false; }
void Player::resume() { if (currentClip_ >= 0) playing_ = true; }

void Player::stop() {
    playing_ = false;
    position_ = 0.0f;
    previousPosition_ = 0.0f;
    transitionTime_ = -1.0f;
    previousClip_ = -1;
    speaking_ = false;
    lipPosition_ = 0.0f;
    if (currentClip_ >= 0 && buffersReady_) {
        sampleClip(clips_[static_cast<size_t>(currentClip_)], skeleton_, 0.0f, &currentPose_, &cacheA_);
        currentPose_.computeMatrices(skeleton_);
    }
}

void Player::resetToBindPose() {
    playing_ = false;
    currentClip_ = -1;
    previousClip_ = -1;
    transitionTime_ = -1.0f;
    position_ = 0.0f;
    previousPosition_ = 0.0f;
    speaking_ = false;
    lipPosition_ = 0.0f;
    ensureBuffers();
    /* resetToRest + computeMatrices rather than advance(0): advance() would re-select nothing but still
       run the layer, face and root-motion stages, and the bind pose must not depend on what a layer
       happens to be doing. */
    currentPose_.resetToRest(skeleton_);
    currentPose_.computeMatrices(skeleton_);
}

void Player::seek(float seconds) {
    if (currentClip_ < 0) return;
    const float duration = currentDuration();
    position_ = (duration > 0.0f) ? clampf(std::isfinite(seconds) ? seconds : 0.0f, 0.0f, duration) : 0.0f;
    previousPosition_ = position_;
    /* Seeking does not implicitly resume: a paused scrub must stay paused, or dragging the timeline
       would start playback the user did not ask for. */
    if (buffersReady_) advance(0.0f);
}

void Player::setSpeed(float speed) {
    speed_ = (std::isfinite(speed) && speed > 0.0f) ? clampf(speed, 0.05f, 8.0f) : 1.0f;
}

void Player::setLoop(bool loop) {
    looping_ = loop;
    if (currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size())) {
        clips_[static_cast<size_t>(currentClip_)].looping = loop;
    }
}

bool Player::crossfadeTo(int clipIndex, float seconds) {
    if (clipIndex < 0 || clipIndex >= static_cast<int>(clips_.size())) return false;
    if (clipIndex == currentClip_) return true;
    ensureBuffers();
    if (!crossfadeEnabled_ || !(seconds > 0.0f) || !std::isfinite(seconds)) {
        return play(clipIndex);
    }
    previousClip_ = currentClip_;
    previousPosition_ = position_;
    currentClip_ = clipIndex;
    position_ = 0.0f;
    transitionDuration_ = seconds;
    transitionTime_ = 0.0f;
    playing_ = true;
    looping_ = clips_[static_cast<size_t>(clipIndex)].looping;
    cacheA_.bind(clips_[static_cast<size_t>(clipIndex)]);
    if (previousClip_ >= 0 && previousClip_ < static_cast<int>(clips_.size())) {
        cacheB_.bind(clips_[static_cast<size_t>(previousClip_)]);
    }
    return true;
}

bool Player::crossfadeTo(const std::string& clipName, float seconds) {
    return crossfadeTo(clipIndex(clipName), seconds);
}

/* ============================================================================ state machine ==== */

bool Player::buildStateMachine(float crossfadeSeconds) {
    machine_ = StateMachine();
    if (clips_.empty()) return false;
    const float fade = std::isfinite(crossfadeSeconds) ? std::max(0.0f, crossfadeSeconds) : 0.25f;
    for (size_t i = 0; i < clips_.size(); ++i) {
        /* Two clips can share a name in a file that declares no animation names; disambiguate rather
           than lose a state, because a state machine that silently drops a clip looks like a clip
           that does not play. */
        std::string name = clips_[i].name.empty() ? ("clip_" + std::to_string(i)) : clips_[i].name;
        if (machine_.stateIndex(name) >= 0) name += "_" + std::to_string(i);
        machine_.addState(name, static_cast<int>(i), clips_[i].looping);
    }
    machine_.addAllTransitions(fade);
    machineDriven_ = true;
    machine_.reset(0);
    currentClip_ = machine_.clipForState(0);
    position_ = 0.0f;
    playing_ = true;
    if (currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size())) {
        cacheA_.bind(clips_[static_cast<size_t>(currentClip_)]);
        looping_ = clips_[static_cast<size_t>(currentClip_)].looping;
    }
    return true;
}

void Player::setStateMachineDriven(bool driven) { machineDriven_ = driven && machine_.stateCount() > 0; }

bool Player::requestState(const std::string& name) {
    if (machine_.stateCount() == 0 && !buildStateMachine(0.25f)) return false;
    machineDriven_ = true;
    const bool ok = machine_.requestState(name);
    if (!ok) return false;
    const int clip = machine_.clipForState(machine_.currentState());
    if (clip >= 0 && clip < static_cast<int>(clips_.size())) {
        currentClip_ = clip;
        cacheA_.bind(clips_[static_cast<size_t>(clip)]);
    }
    return true;
}

/* ============================================================================ lip-sync ========= */

void Player::speak(const std::string& text, float totalSeconds) {
    if (text.empty()) { stopSpeaking(); return; }
    if (std::isfinite(totalSeconds) && totalSeconds > 0.0f) {
        lipTrack_.buildForDuration(text, totalSeconds);
    } else {
        lipTrack_.buildFromText(text);
    }
    if (lipTrack_.keyCount() == 0) { stopSpeaking(); return; }
    lipPosition_ = 0.0f;
    speaking_ = true;
    face_.setLipSyncEnabled(true);
}

void Player::stopSpeaking() {
    speaking_ = false;
    lipPosition_ = 0.0f;
    face_.setLipSyncEnabled(false);
    lipTrack_.clear();
}

/* ================================================================================= layers ====== */

int Player::addLayer(int clipIndex, float weight, bool additive) {
    if (clipIndex < 0 || clipIndex >= static_cast<int>(clips_.size())) return -1;
    ensureBuffers();
    Layer l;
    l.clip = clipIndex;
    l.weight = saturate(std::isfinite(weight) ? weight : 0.0f);
    l.additive = additive;
    l.enabled = false;
    l.position = 0.0f;
    l.speed = 1.0f;
    l.mask = BoneMask::upperBody(skeleton_);
    l.pose.allocate(skeleton_, morphCount_);
    l.cache.bind(clips_[static_cast<size_t>(clipIndex)]);
    layers_.push_back(std::move(l));
    return static_cast<int>(layers_.size()) - 1;
}

Layer* Player::layerAt(int index) {
    if (index < 0 || index >= static_cast<int>(layers_.size())) return nullptr;
    return &layers_[static_cast<size_t>(index)];
}

bool Player::setLayerClip(int index, int clipIndex) {
    Layer* l = layerAt(index);
    if (!l || clipIndex < 0 || clipIndex >= static_cast<int>(clips_.size())) return false;
    l->clip = clipIndex;
    l->position = 0.0f;
    l->cache.bind(clips_[static_cast<size_t>(clipIndex)]);
    return true;
}

bool Player::setLayerWeight(int index, float weight) {
    Layer* l = layerAt(index);
    if (!l) return false;
    l->weight = saturate(std::isfinite(weight) ? weight : 0.0f);
    return true;
}

bool Player::setLayerEnabled(int index, bool enabled) {
    Layer* l = layerAt(index);
    if (!l) return false;
    l->enabled = enabled;
    return true;
}

bool Player::setLayerMask(int index, const BoneMask& mask) {
    Layer* l = layerAt(index);
    if (!l || mask.include.size() != static_cast<size_t>(skeleton_.count())) return false;
    l->mask = mask;
    return true;
}

bool Player::setLayerAdditive(int index, bool additive) {
    Layer* l = layerAt(index);
    if (!l) return false;
    l->additive = additive;
    return true;
}

bool Player::addEvent(int clipIndex, float time, const std::string& name, float value) {
    Clip* c = (clipIndex >= 0 && clipIndex < static_cast<int>(clips_.size()))
                  ? &clips_[static_cast<size_t>(clipIndex)] : nullptr;
    if (!c || name.empty() || !std::isfinite(time)) return false;
    Event e;
    e.time = clampf(time, 0.0f, c->duration > 0.0f ? c->duration : time);
    e.name = name;
    e.value = value;
    c->events.push_back(e);
    std::stable_sort(c->events.begin(), c->events.end(),
                     [](const Event& a, const Event& b) { return a.time < b.time; });
    return true;
}

/* ============================================================================ retargeting ====== */

bool Player::checkCompatibility(const Skeleton& other, RetargetMap* outReport) const {
    RetargetMap local;
    RetargetMap* target = outReport ? outReport : &local;
    /* Direction: `other` is the source of motion, this player's skeleton is the destination. */
    return buildRetargetMap(other, skeleton_, target);
}

int Player::retarget(const Skeleton& srcSkeleton, const Clip& srcClip, std::string* error) {
    auto fail = [&](const std::string& msg) { if (error) *error = msg; return -1; };
    if (skeleton_.count() == 0) return fail("no skeleton is loaded to retarget onto");
    if (srcSkeleton.count() == 0) return fail("the source skeleton is empty");
    if (srcClip.channels.empty()) return fail("the source clip has no channels");

    RetargetMap map;
    if (!buildRetargetMap(srcSkeleton, skeleton_, &map)) {
        std::string msg = "the skeletons are not retarget-compatible (coverage " +
                          std::to_string(static_cast<int>(map.coverage * 100.0f)) + "%";
        if (!map.missingRoles.empty()) {
            msg += "; missing: ";
            for (size_t i = 0; i < map.missingRoles.size() && i < 8; ++i) {
                if (i) msg += ", ";
                msg += map.missingRoles[i];
            }
            if (map.missingRoles.size() > 8) msg += ", ...";
        }
        msg += ")";
        return fail(msg);
    }
    ensureBuffers();

    /* Bake by resampling: collect every key time the source clip uses, evaluate the source pose and
       the retargeted destination pose at each, and emit Linear rotation channels from the results.
       Baking rather than retargeting at playback costs memory once and keeps the per-frame path the
       same as for a native clip, which is what makes a retargeted animation usable as a layer, a
       state or a crossfade target without a second code path. */
    std::set<float> times;
    for (const Channel& ch : srcClip.channels) {
        if (ch.path == Path::Weights) continue;
        for (float t : ch.times) times.insert(clampf(t, 0.0f, srcClip.duration));
    }
    if (times.empty()) times.insert(0.0f);
    times.insert(srcClip.duration > 0.0f ? srcClip.duration : 0.0f);

    const int n = skeleton_.count();
    std::vector<std::vector<float>> keyTimes(static_cast<size_t>(n));
    std::vector<std::vector<Quat>> keyRots(static_cast<size_t>(n));
    std::vector<float> hipsTimes;
    std::vector<Vec3> hipsTranslations;
    const int dstHips = skeleton_.jointForRole(Role::Hips);

    Pose srcPose, dstPose;
    srcPose.allocate(srcSkeleton, 0);
    dstPose.allocate(skeleton_, 0);
    SampleCache srcCache;
    srcCache.bind(srcClip);
    for (float t : times) {
        sampleClip(srcClip, srcSkeleton, t, &srcPose, &srcCache);
        retargetPose(srcSkeleton, skeleton_, map, srcPose, &dstPose);
        for (int j = 0; j < n; ++j) {
            if (map.dstToSrc[static_cast<size_t>(j)] < 0) continue;
            keyTimes[static_cast<size_t>(j)].push_back(t);
            keyRots[static_cast<size_t>(j)].push_back(dstPose.r[static_cast<size_t>(j)]);
        }
        if (dstHips >= 0) {
            hipsTimes.push_back(t);
            hipsTranslations.push_back(dstPose.t[static_cast<size_t>(dstHips)]);
        }
    }

    Clip baked;
    baked.name = srcClip.name + " (retargeted)";
    baked.duration = srcClip.duration;
    baked.looping = srcClip.looping;
    baked.authored = false;
    baked.sourceAnimation = -1;
    baked.category = srcClip.category;
    for (int j = 0; j < n; ++j) {
        if (keyTimes[static_cast<size_t>(j)].size() < 2) continue;
        Channel ch;
        ch.joint = j;
        ch.path = Path::Rotation;
        ch.interp = Interp::Linear;
        ch.components = 4;
        ch.times = keyTimes[static_cast<size_t>(j)];
        ch.values.reserve(keyRots[static_cast<size_t>(j)].size());
        for (const Quat& q : keyRots[static_cast<size_t>(j)]) ch.values.push_back(Vec4(q.x, q.y, q.z, q.w));
        baked.channels.push_back(std::move(ch));
    }
    if (dstHips >= 0 && hipsTimes.size() >= 2) {
        Channel ch;
        ch.joint = dstHips;
        ch.path = Path::Translation;
        ch.interp = Interp::Linear;
        ch.components = 3;
        ch.times = hipsTimes;
        ch.values.reserve(hipsTranslations.size());
        for (const Vec3& v : hipsTranslations) ch.values.push_back(Vec4(v.x, v.y, v.z, 0.0f));
        baked.channels.push_back(std::move(ch));
        baked.hasRootTranslation = true;
    }
    if (baked.channels.empty()) return fail("retargeting produced no usable channel");

    char buf[224];
    std::snprintf(buf, sizeof(buf),
                  "retargeted '%s': %d of %d destination joints driven, coverage %.0f%%, "
                  "translation scale %.3f",
                  srcClip.name.c_str(), map.mappedJoints(), n, map.coverage * 100.0f, map.scale);
    loadNotes_.push_back(buf);
    for (const std::string& note : map.notes) loadNotes_.push_back(note);
    for (const std::string& missing : map.missingRoles) {
        loadNotes_.push_back("retarget: role not matched - " + missing);
    }
    return addClip(std::move(baked));
}

/* ============================================================================== per-frame ====== */

void Player::applyClipToPose(const Clip& clip, float time, SampleCache* cache, Pose* out) {
    sampleClip(clip, skeleton_, time, out, cache);
}

void Player::fireEvents(const Clip& clip, float from, float to) {
    if (!eventListener_ || clip.events.empty()) return;
    if (to >= from) {
        for (const Event& e : clip.events) {
            if (e.time > from && e.time <= to) eventListener_(e.name, e.value, e.time);
        }
    } else {
        /* The clock wrapped: fire the tail of the loop and then its head, so an event at the very end
           of a cycle is not skipped by the wrap. */
        for (const Event& e : clip.events) {
            if (e.time > from) eventListener_(e.name, e.value, e.time);
        }
        for (const Event& e : clip.events) {
            if (e.time <= to) eventListener_(e.name, e.value, e.time);
        }
    }
}

void Player::advance(float dt) {
    const double t0 = nowMicros();
    const float step = sanitiseDt(dt);
    ensureBuffers();

    const float previousPosition = position_;

    if (machineDriven_ && machine_.stateCount() > 0) {
        /* The state machine owns the clocks. Its clip indices are authoritative, so a state change
           made by another thread of control (the UI, a game rule) is picked up here. */
        machine_.advance(step * speed_, clips_);
        const StateMachine::Output o = machine_.output();
        if (o.clipA >= 0 && o.clipA < static_cast<int>(clips_.size())) {
            if (currentClip_ != o.clipA) {
                currentClip_ = o.clipA;
                cacheA_.bind(clips_[static_cast<size_t>(o.clipA)]);
            }
            position_ = o.timeA;
        }
        if (o.transitioning && o.clipB >= 0 && o.clipB < static_cast<int>(clips_.size())) {
            previousClip_ = o.clipB;
            previousPosition_ = o.timeB;
            transitionDuration_ = machine_.transitionDuration();
            transitionTime_ = o.alpha * transitionDuration_;
            cacheB_.bind(clips_[static_cast<size_t>(o.clipB)]);
        } else {
            previousClip_ = -1;
            transitionTime_ = -1.0f;
        }
    } else {
        if (playing_ && currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size())) {
            const Clip& clip = clips_[static_cast<size_t>(currentClip_)];
            position_ = wrapTime(position_ + step * speed_, clip.duration);
            if (!looping_ && clip.duration > 0.0f && position_ >= clip.duration - kEps) {
                position_ = clip.duration;
                playing_ = false;                 /* a non-looping clip ends itself, visibly */
            }
        }
        if (transitionTime_ >= 0.0f) {
            /* Crossfade progress runs on wall time, not on the playback speed: speeding up a clip
               should not also shorten the blend the user asked for. */
            transitionTime_ += step;
            if (previousClip_ >= 0 && previousClip_ < static_cast<int>(clips_.size())) {
                const Clip& prev = clips_[static_cast<size_t>(previousClip_)];
                previousPosition_ = wrapTime(previousPosition_ + step * speed_, prev.duration);
            }
            if (transitionDuration_ <= 0.0f || transitionTime_ >= transitionDuration_) {
                transitionTime_ = -1.0f;
                previousClip_ = -1;
                previousPosition_ = 0.0f;
            }
        }
    }

    /* ---- events ---- */
    if (currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size())) {
        fireEvents(clips_[static_cast<size_t>(currentClip_)], previousPosition, position_);
    }

    /* ---- root motion ---- */
    if (rootMotion_.enabled && currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size())) {
        const Clip& clip = clips_[static_cast<size_t>(currentClip_)];
        if (clip.hasRootTranslation && step > 0.0f) {
            rootMotion_.accumulated = rootMotion_.accumulated +
                rootTranslationDelta(skeleton_, clip, previousPosition, position_, nullptr);
        }
    }

    /* ---- 1. base pose ---- */
    if (currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size())) {
        const Clip& clip = clips_[static_cast<size_t>(currentClip_)];
        const bool blending = transitionTime_ >= 0.0f && previousClip_ >= 0 &&
                              previousClip_ < static_cast<int>(clips_.size()) && crossfadeEnabled_;
        if (blending) {
            const Clip& prev = clips_[static_cast<size_t>(previousClip_)];
            const float alpha = transitionDuration_ > 0.0f
                                    ? saturate(transitionTime_ / transitionDuration_)
                                    : 1.0f;
            sampleClip(clip, skeleton_, position_, &scratchA_, &cacheA_);
            sampleClip(prev, skeleton_, previousPosition_, &scratchB_, &cacheB_);
            stats_.posesSampled += 2;
            /* thread_local and resized, not constructed: a two-element vector built here every frame
               is two heap allocations per frame on the render thread, which is exactly what this
               module's allocation policy forbids. */
            thread_local std::vector<BlendEntry> entries;
            entries.resize(2);
            entries[0].pose = &scratchA_;
            entries[0].weight = alpha;
            entries[1].pose = &scratchB_;
            entries[1].weight = 1.0f - alpha;
            blendPoses(entries, &currentPose_);
            /* Morphs are blended with the same alpha: blendPoses() handles joints only. */
            blendMorphWeights(scratchB_.morphs, scratchA_.morphs, alpha, &currentPose_.morphs);
        } else {
            sampleClip(clip, skeleton_, position_, &currentPose_, &cacheA_);
            stats_.posesSampled += 1;
        }
    } else {
        currentPose_.resetToRest(skeleton_);
    }

    /* ---- 2. layers ---- */
    for (Layer& l : layers_) {
        if (!l.enabled || !(l.weight > 0.0f)) continue;
        if (l.clip < 0 || l.clip >= static_cast<int>(clips_.size())) continue;
        const Clip& clip = clips_[static_cast<size_t>(l.clip)];
        if (playing_) l.position = wrapTime(l.position + step * speed_ * l.speed, clip.duration);
        sampleClip(clip, skeleton_, l.position, &l.pose, &l.cache);
        stats_.posesSampled += 1;
        if (l.additive) blendAdditive(skeleton_, currentPose_, l.pose, l.weight, l.mask, &currentPose_);
        else blendMasked(currentPose_, l.pose, l.weight, l.mask, &currentPose_);
    }

    /* ---- 3. face: blink, then lip-sync, then merge onto the clip's own morph track ---- */
    if (speaking_ && face_.lipSyncEnabled() && lipTrack_.duration() > 0.0f) {
        lipPosition_ += step;
        if (lipPosition_ > lipTrack_.duration()) {
            lipPosition_ = lipTrack_.duration();
            speaking_ = false;
            face_.setLipSyncEnabled(false);
        }
        float amount = 0.0f;
        const Viseme v = lipTrack_.sample(lipPosition_, &amount);
        face_.applyViseme(v, amount);
    }
    face_.updateBlink(step);

    if (face_.morphCount() > 0 && static_cast<int>(currentPose_.morphs.size()) == face_.morphCount()) {
        const std::vector<float>& fw = face_.weights();
        for (size_t m = 0; m < fw.size(); ++m) {
            /* max(): an authored morph track and a live control both want the channel, and taking the
               stronger keeps a blink visible over an idle face track without either cancelling the
               other. Documented in the header of this file. */
            if (fw[m] > currentPose_.morphs[m]) currentPose_.morphs[m] = fw[m];
        }
    }

    /* ---- 4. matrices ---- */
    currentPose_.computeMatrices(skeleton_);

    const double t1 = nowMicros();
    const double elapsed = t1 - t0;
    stats_.lastAdvanceMicros = elapsed;
    stats_.maxAdvanceMicros = elapsed > stats_.maxAdvanceMicros ? elapsed : stats_.maxAdvanceMicros;
    stats_.frames += 1;
    stats_.jointCount = skeleton_.count();
    int channels = 0;
    if (currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size())) {
        channels += static_cast<int>(clips_[static_cast<size_t>(currentClip_)].channels.size());
    }
    stats_.channelCount = channels;
    const double n = static_cast<double>(stats_.frames);
    stats_.avgAdvanceMicros = stats_.avgAdvanceMicros + (elapsed - stats_.avgAdvanceMicros) / n;
}

PlayerState Player::state() const {
    PlayerState s;
    s.clip = currentClip_;
    s.clipName = (currentClip_ >= 0 && currentClip_ < static_cast<int>(clips_.size()))
                     ? clips_[static_cast<size_t>(currentClip_)].name : std::string();
    s.position = position_;
    s.duration = currentDuration();
    s.speed = speed_;
    s.playing = playing_;
    s.looping = looping_;
    s.transitioning = transitionTime_ >= 0.0f;
    s.previousClip = previousClip_;
    s.previousPosition = previousPosition_;
    s.transitionProgress = (transitionTime_ >= 0.0f && transitionDuration_ > 0.0f)
                               ? saturate(transitionTime_ / transitionDuration_) : 1.0f;
    s.layerCount = static_cast<int>(layers_.size());
    s.morphCount = morphCount_;
    s.jointCount = skeleton_.count();
    return s;
}

void Player::resetStats() {
    stats_ = Stats();
    stats_.jointCount = skeleton_.count();
}

} /* namespace anim */
} /* namespace af */
