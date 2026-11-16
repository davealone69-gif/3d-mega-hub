/*
 * af_anim_player.h -- what plays when: the clocks, crossfades and the overlay of one character.
 *
 * The engine evaluates poses (af_engine_evaluate_layers); it has no notion of time. A renderer
 * draws; it should not decide how a walk turns into a run. This class sits between them. It holds
 *
 *   a BLEND STACK   the animation that plays, and under it the ones it is still fading in over.
 *                   A crossfade pushes the new animation on top with a weight that rises from 0
 *                   to 1; when it reaches 1 everything under it is dropped. A fade that is
 *                   interrupted by another one therefore stays smooth: nothing is cut.
 *   an OVERLAY      one more animation on part of the skeleton (a wave over a walk, a hand on a
 *                   hat), with its own clock and its own fade in and out. An overlay that does
 *                   not loop leaves by itself when it reaches its end: a one-shot gesture.
 *
 * and each frame it turns them into the list of layers the engine blends. It has no dependency on
 * the engine, on OpenGL or on Android: durations are passed in, so the same class runs in the
 * GLES renderer and in a host test.
 */
#ifndef AF_ANIM_PLAYER_H
#define AF_ANIM_PLAYER_H

#include "afcore.h"

namespace af {

/* What a user interface is told about playback. */
struct AnimationPlaybackState {
    int   animation = -1;        /* the animation on top of the stack; -1 is the rest pose */
    float time = 0.0f;           /* seconds into it */
    float duration = 0.0f;
    float speed = 1.0f;
    bool  loop = true;
    bool  paused = false;
    bool  finished = false;      /* it does not loop and has reached its end */
    int   loops = 0;             /* times it has wrapped since it started */
    bool  fading = false;        /* a crossfade is running */
    int   fadingFrom = -1;       /* the animation directly under the top one while fading */
    float fadeProgress = 1.0f;   /* 0 at the start of a crossfade, 1 when done */
    int   transitions = 0;       /* crossfades completed since reset(): an event counter */
    int   overlayAnimation = -1;
    int   overlayMask = 0;
    float overlayWeight = 0.0f;  /* its weight now, after its fade */
    float overlayTime = 0.0f;
    int   layers = 0;            /* layers the last advance() produced */
};

class AnimationPlayer {
public:
    static constexpr int kMaxStack = 4;
    /* The stack plus the overlay: the most layers advance() can write. */
    static constexpr int kMaxLayers = kMaxStack + 1;

    /** Everything stops: rest pose, no overlay, counters at zero. For a new model. */
    void reset();

    /** Rest pose at once, no overlay. The transition counter keeps counting. */
    void stop();

    /** Plays `animation` from its start at once, with no fade. -1 returns to the rest pose. */
    void play(int animation, float duration, float speed, bool loop);

    /** Plays `animation` from its start and fades it in over `fadeSeconds` on top of whatever
        plays now (the rest pose included). A fade of zero or less is play(). */
    void crossFade(int animation, float duration, float fadeSeconds, float speed, bool loop);

    /** Speed and looping of the top animation, and the pause of everything. */
    void setPlayback(float speed, bool loop, bool paused);

    /** Moves the top animation to `seconds`. A scrubbed pose is the clip alone: a running
        crossfade is finished at once. */
    void seek(float seconds);

    /** Puts `animation` on the joints of `mask` (an af_joint_mask) at `weight`, fading in over
        `fadeSeconds`. -1 fades the present overlay out over `fadeSeconds`. */
    void setOverlay(int animation, float duration, int mask, float weight, float speed, bool loop, float fadeSeconds);

    /**
     * Advances every clock by `deltaSeconds` (nothing moves while paused) and writes the layers
     * to blend, bottom first. Returns how many were written: 0 means the rest pose, and the
     * caller need not skin at all.
     */
    int advance(float deltaSeconds, af_animation_layer* outLayers, int maxLayers);

    AnimationPlaybackState state() const;
    bool active() const { return stackCount_ > 0 || overlay_.animation >= 0; }

private:
    struct Track {
        int   animation = -1;
        float duration = 0.0f;
        float time = 0.0f;
        float speed = 1.0f;
        bool  loop = true;
        float alpha = 1.0f;        /* how far its fade-in has come, 0..1 (linear in time) */
        float fadeSeconds = 0.0f;
        int   loops = 0;
        bool  finished = false;
    };
    static void advanceTrack(Track& t, float dt);
    static float clampSpeed(float speed);

    Track stack_[kMaxStack];
    int   stackCount_ = 0;
    bool  paused_ = false;
    int   transitions_ = 0;
    Track overlay_;                /* alpha is its fade, 0..1 */
    int   overlayMask_ = 0;
    float overlayWeight_ = 0.0f;   /* the weight asked for */
    bool  overlayLeaving_ = false;
    int   lastLayers_ = 0;
};

} // namespace af
#endif /* AF_ANIM_PLAYER_H */
