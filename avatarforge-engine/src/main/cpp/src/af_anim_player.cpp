/*
 * af_anim_player.cpp -- see af_anim_player.h.
 */
#include "af_anim_player.h"

#include <algorithm>
#include <cmath>

namespace af {

namespace {

inline float smooth01(float x) {
    const float t = std::max(0.0f, std::min(1.0f, x));
    return t * t * (3.0f - 2.0f * t);
}

inline bool usable(float v) { return std::isfinite(v); }

} // namespace

float AnimationPlayer::clampSpeed(float speed) {
    return usable(speed) ? std::max(0.0f, std::min(4.0f, speed)) : 1.0f;
}

void AnimationPlayer::reset() {
    stop();
    transitions_ = 0;
}

void AnimationPlayer::stop() {
    stackCount_ = 0;
    paused_ = false;
    overlay_ = Track{};
    overlayMask_ = 0;
    overlayWeight_ = 0.0f;
    overlayLeaving_ = false;
    lastLayers_ = 0;
}

void AnimationPlayer::play(int animation, float duration, float speed, bool loop) {
    stackCount_ = 0;
    paused_ = false;
    if (animation < 0) return;
    Track t;
    t.animation = animation;
    t.duration = usable(duration) && duration > 0.0f ? duration : 0.0f;
    t.speed = clampSpeed(speed);
    t.loop = loop;
    t.alpha = 1.0f;
    stack_[0] = t;
    stackCount_ = 1;
}

void AnimationPlayer::crossFade(int animation, float duration, float fadeSeconds, float speed, bool loop) {
    if (!usable(fadeSeconds) || fadeSeconds <= 0.0f || (stackCount_ == 0 && animation < 0)) {
        play(animation, duration, speed, loop);
        return;
    }
    paused_ = false;
    if (stackCount_ == 0) {
        /* Fading in from the rest pose: the rest pose is the bottom of the stack. */
        stack_[0] = Track{};
        stackCount_ = 1;
    }
    if (stackCount_ == kMaxStack) {
        /* Four fades inside one another: the oldest, by now almost covered, is let go. */
        for (int i = 1; i < kMaxStack; ++i) stack_[i - 1] = stack_[i];
        stack_[0].alpha = 1.0f;
        --stackCount_;
    }
    Track t;
    t.animation = animation < 0 ? -1 : animation;
    t.duration = animation >= 0 && usable(duration) && duration > 0.0f ? duration : 0.0f;
    t.speed = clampSpeed(speed);
    t.loop = loop;
    t.alpha = 0.0f;
    t.fadeSeconds = fadeSeconds;
    stack_[stackCount_++] = t;
}

void AnimationPlayer::setPlayback(float speed, bool loop, bool paused) {
    paused_ = paused;
    if (stackCount_ == 0) return;
    Track& top = stack_[stackCount_ - 1];
    top.speed = clampSpeed(speed);
    if (loop && !top.loop) top.finished = false;
    top.loop = loop;
}

void AnimationPlayer::seek(float seconds) {
    if (stackCount_ == 0 || !usable(seconds)) return;
    Track top = stack_[stackCount_ - 1];
    if (stackCount_ > 1) ++transitions_;
    top.alpha = 1.0f;
    top.time = top.duration > 0.0f ? std::max(0.0f, std::min(top.duration, seconds)) : 0.0f;
    top.finished = !top.loop && top.duration > 0.0f && top.time >= top.duration;
    stack_[0] = top;
    stackCount_ = top.animation >= 0 ? 1 : 0;
}

void AnimationPlayer::setOverlay(int animation, float duration, int mask, float weight, float speed, bool loop, float fadeSeconds) {
    const float fade = usable(fadeSeconds) && fadeSeconds > 0.0f ? fadeSeconds : 0.0f;
    if (animation < 0) {
        if (overlay_.animation < 0) return;
        overlay_.fadeSeconds = fade;
        overlayLeaving_ = true;
        if (fade <= 0.0f) { overlay_ = Track{}; overlayLeaving_ = false; overlayWeight_ = 0.0f; }
        return;
    }
    /* A new overlay starts where the old one's fade stands, so swapping gestures does not pop
       the arm back to the base animation and out again. */
    const float carried = overlay_.animation >= 0 ? overlay_.alpha : 0.0f;
    overlay_ = Track{};
    overlay_.animation = animation;
    overlay_.duration = usable(duration) && duration > 0.0f ? duration : 0.0f;
    overlay_.speed = clampSpeed(speed);
    overlay_.loop = loop;
    overlay_.fadeSeconds = fade;
    overlay_.alpha = fade > 0.0f ? carried : 1.0f;
    overlayMask_ = mask >= 0 && mask < AF_MASK_COUNT ? mask : AF_MASK_FULL_BODY;
    overlayWeight_ = usable(weight) ? std::max(0.0f, std::min(1.0f, weight)) : 1.0f;
    overlayLeaving_ = false;
}

void AnimationPlayer::advanceTrack(Track& t, float dt) {
    if (t.animation < 0 || t.duration <= 0.0f) { t.time = 0.0f; return; }
    t.time += dt * t.speed;
    if (t.loop) {
        if (t.time >= t.duration) {
            const float wraps = std::floor(t.time / t.duration);
            t.loops += static_cast<int>(wraps);
            t.time -= wraps * t.duration;
            if (t.time < 0.0f || t.time >= t.duration) t.time = 0.0f;
        }
        t.finished = false;
    } else if (t.time >= t.duration) {
        t.time = t.duration;
        t.finished = true;
    }
}

int AnimationPlayer::advance(float deltaSeconds, af_animation_layer* outLayers, int maxLayers) {
    const float dt = (!paused_ && usable(deltaSeconds) && deltaSeconds > 0.0f) ? std::min(deltaSeconds, 1.0f) : 0.0f;

    for (int i = 0; i < stackCount_; ++i) {
        advanceTrack(stack_[i], dt);
        if (i > 0 && stack_[i].alpha < 1.0f) {
            stack_[i].alpha = stack_[i].fadeSeconds > 0.0f ? std::min(1.0f, stack_[i].alpha + dt / stack_[i].fadeSeconds) : 1.0f;
        }
    }
    /* The highest track that has faded in completely hides everything under it. */
    int keepFrom = 0;
    for (int i = stackCount_ - 1; i > 0; --i) {
        if (stack_[i].alpha >= 1.0f) { keepFrom = i; break; }
    }
    if (keepFrom > 0) {
        transitions_ += 1;
        for (int i = keepFrom; i < stackCount_; ++i) stack_[i - keepFrom] = stack_[i];
        stackCount_ -= keepFrom;
        stack_[0].alpha = 1.0f;
    }
    /* A stack that has come down to the rest pose alone is no stack. */
    if (stackCount_ == 1 && stack_[0].animation < 0) stackCount_ = 0;

    if (overlay_.animation >= 0) {
        advanceTrack(overlay_, dt);
        if (overlay_.finished && !overlayLeaving_) overlayLeaving_ = true;      /* a one-shot gesture is over */
        if (overlayLeaving_) {
            overlay_.alpha = overlay_.fadeSeconds > 0.0f ? overlay_.alpha - dt / overlay_.fadeSeconds : 0.0f;
            if (overlay_.alpha <= 0.0f) {
                overlay_ = Track{};
                overlayLeaving_ = false;
                overlayWeight_ = 0.0f;
            }
        } else if (overlay_.alpha < 1.0f) {
            overlay_.alpha = overlay_.fadeSeconds > 0.0f ? std::min(1.0f, overlay_.alpha + dt / overlay_.fadeSeconds) : 1.0f;
        }
    }

    int n = 0;
    if (outLayers && maxLayers > 0) {
        for (int i = 0; i < stackCount_ && n < maxLayers; ++i) {
            const Track& t = stack_[i];
            if (i == 0 && t.animation < 0) continue;      /* the pose starts as the rest pose */
            af_animation_layer layer;
            layer.animation = t.animation;
            layer.time_seconds = t.time;
            layer.weight = i == 0 ? 1.0f : smooth01(t.alpha);
            layer.mask = AF_MASK_FULL_BODY;
            outLayers[n++] = layer;
        }
        if (overlay_.animation >= 0 && n < maxLayers) {
            const float weight = overlayWeight_ * smooth01(overlay_.alpha);
            if (weight > 0.0f) {
                af_animation_layer layer;
                layer.animation = overlay_.animation;
                layer.time_seconds = overlay_.time;
                layer.weight = weight;
                layer.mask = overlayMask_;
                outLayers[n++] = layer;
            }
        }
    }
    lastLayers_ = n;
    return n;
}

AnimationPlaybackState AnimationPlayer::state() const {
    AnimationPlaybackState s;
    s.paused = paused_;
    s.transitions = transitions_;
    s.layers = lastLayers_;
    if (stackCount_ > 0) {
        const Track& top = stack_[stackCount_ - 1];
        s.animation = top.animation;
        s.time = top.time;
        s.duration = top.duration;
        s.speed = top.speed;
        s.loop = top.loop;
        s.finished = top.finished;
        s.loops = top.loops;
        s.fading = stackCount_ > 1;
        s.fadingFrom = stackCount_ > 1 ? stack_[stackCount_ - 2].animation : -1;
        s.fadeProgress = stackCount_ > 1 ? top.alpha : 1.0f;
    }
    if (overlay_.animation >= 0) {
        s.overlayAnimation = overlay_.animation;
        s.overlayMask = overlayMask_;
        s.overlayWeight = overlayWeight_ * smooth01(overlay_.alpha);
        s.overlayTime = overlay_.time;
    }
    return s;
}

} // namespace af
