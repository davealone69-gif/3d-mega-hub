package com.davemega.hub.avatarforge.animation

/**
 * The joint masks of the engine (af_joint_mask), in its order: [id] is what the renderer takes.
 *
 * A note on sides. The rig names its joints as a front picture shows them: the joints named _L
 * are on the avatar's own right. The labels here are the avatar's own sides, which is what a
 * person means by "her right arm".
 */
enum class JointMask(val label: String) {
    FULL_BODY("Whole body"),
    UPPER_BODY("Upper body"),
    LOWER_BODY("Hips and legs"),
    /** AF_MASK_LEFT_ARM: the joints named _L. */
    OWN_RIGHT_ARM("Right arm"),
    /** AF_MASK_RIGHT_ARM: the joints named _R. */
    OWN_LEFT_ARM("Left arm"),
    ARMS("Both arms"),
    HEAD("Head and neck");

    val id: Int get() = ordinal
}

/** Playback as the renderer has it, read once a frame. */
data class AnimationPlaybackState(
    /** The animation that plays, or the one fading in; [NONE] at rest. */
    val animation: Int = NONE,
    val timeSeconds: Float = 0f,
    val durationSeconds: Float = 0f,
    val speed: Float = 1f,
    val loop: Boolean = true,
    val paused: Boolean = false,
    /** It does not loop and has reached its end. */
    val finished: Boolean = false,
    /** Times it has started over since it began. */
    val loops: Int = 0,
    /** A crossfade is running, from [fadingFrom] ([NONE] is the rest pose), [fadeProgress] of the way. */
    val fading: Boolean = false,
    val fadingFrom: Int = NONE,
    val fadeProgress: Float = 1f,
    /** Crossfades completed since the model was loaded. */
    val transitions: Int = 0,
    val overlayAnimation: Int = NONE,
    val overlayMask: Int = 0,
    val overlayWeight: Float = 0f,
    val overlayTimeSeconds: Float = 0f,
    /** Animations blended into the last frame. */
    val layers: Int = 0,
    /** Face kernels the last frame evaluated: 0 when the face is at rest. */
    val faceKernels: Int = 0
) {
    val playing: Boolean get() = animation != NONE || overlayAnimation != NONE

    companion object {
        const val NONE = -1
        /** Floats the renderer writes for one state, in the order of the constructor. */
        const val NATIVE_SIZE = 18

        fun fromNative(values: FloatArray): AnimationPlaybackState {
            if (values.size < NATIVE_SIZE) return AnimationPlaybackState()
            return AnimationPlaybackState(
                animation = values[0].toInt(),
                timeSeconds = values[1],
                durationSeconds = values[2],
                speed = values[3],
                loop = values[4] > 0.5f,
                paused = values[5] > 0.5f,
                finished = values[6] > 0.5f,
                loops = values[7].toInt(),
                fading = values[8] > 0.5f,
                fadingFrom = values[9].toInt(),
                fadeProgress = values[10],
                transitions = values[11].toInt(),
                overlayAnimation = values[12].toInt(),
                overlayMask = values[13].toInt(),
                overlayWeight = values[14],
                overlayTimeSeconds = values[15],
                layers = values[16].toInt(),
                faceKernels = values[17].toInt()
            )
        }
    }
}

/** What a caller can drive: the renderer view implements it, and a test can stand in for it. */
interface AnimationController {
    /** Starts [index] and fades it in over [fadeSeconds]; [AnimationPlaybackState.NONE] fades to the rest pose. */
    fun crossFadeTo(index: Int, fadeSeconds: Float, loop: Boolean = true, speed: Float = 1f)

    /** A second animation on part of the body, over the one that plays. */
    fun setOverlay(index: Int, mask: JointMask, weight: Float = 1f, loop: Boolean = true, fadeSeconds: Float = 0.2f, speed: Float = 1f)

    fun clearOverlay(fadeSeconds: Float = 0.2f)

    fun setAnimationSpeed(speed: Float)
}

/** Something that happened in playback, found by comparing one state with the next. */
sealed class AnimationEvent {
    data class Started(val animation: Int) : AnimationEvent()
    /** The model returned to the rest pose; [animation] is what played before. */
    data class Stopped(val animation: Int) : AnimationEvent()
    data class Looped(val animation: Int, val loops: Int) : AnimationEvent()
    /** An animation that does not loop reached its end. */
    data class Finished(val animation: Int) : AnimationEvent()
    data class TransitionStarted(val from: Int, val to: Int) : AnimationEvent()
    data class TransitionCompleted(val to: Int) : AnimationEvent()
    data class OverlayStarted(val animation: Int, val mask: Int) : AnimationEvent()
    data class OverlayEnded(val animation: Int) : AnimationEvent()
    /** Playback passed a time the caller marked: a footstep, the top of a jump. */
    data class Marker(val animation: Int, val name: String, val timeSeconds: Float) : AnimationEvent()
}

/**
 * Turns successive playback states into events. Feed it the state once a frame (or less often:
 * nothing is lost but the order inside one interval). Markers are times in an animation the
 * caller wants to be told about; one fires when playback passes it, also across a loop.
 */
class AnimationEventTracker {
    private data class MarkerSpec(val animation: Int, val name: String, val timeSeconds: Float)

    private val markers = ArrayList<MarkerSpec>()
    private var previous = AnimationPlaybackState()

    fun addMarker(animation: Int, name: String, timeSeconds: Float) {
        if (animation >= 0 && timeSeconds.isFinite() && timeSeconds >= 0f) markers += MarkerSpec(animation, name, timeSeconds)
    }

    fun clearMarkers() = markers.clear()

    /** Forgets the last state, as after loading another model. */
    fun reset() {
        previous = AnimationPlaybackState()
    }

    fun update(current: AnimationPlaybackState): List<AnimationEvent> {
        val before = previous
        previous = current
        val events = ArrayList<AnimationEvent>()
        val none = AnimationPlaybackState.NONE

        if (current.animation != before.animation) {
            if (current.fading && current.animation != none) {
                events += AnimationEvent.TransitionStarted(current.fadingFrom, current.animation)
            }
            if (current.animation != none) {
                events += AnimationEvent.Started(current.animation)
                fireMarkers(events, current.animation, -1f, current.timeSeconds)
            } else if (current.fading) {
                events += AnimationEvent.TransitionStarted(before.animation, none)
            }
        } else if (current.animation != none) {
            if (current.loops > before.loops) {
                events += AnimationEvent.Looped(current.animation, current.loops)
                fireMarkers(events, current.animation, before.timeSeconds, Float.MAX_VALUE)
                fireMarkers(events, current.animation, -1f, current.timeSeconds)
            } else if (current.loops < before.loops) {
                /* The same animation was started again from its beginning. */
                events += AnimationEvent.Started(current.animation)
                fireMarkers(events, current.animation, -1f, current.timeSeconds)
            } else if (current.timeSeconds > before.timeSeconds) {
                fireMarkers(events, current.animation, before.timeSeconds, current.timeSeconds)
            }
            if (current.finished && !before.finished) events += AnimationEvent.Finished(current.animation)
        }
        if (current.transitions > before.transitions) events += AnimationEvent.TransitionCompleted(current.animation)
        if (current.animation == none && before.animation != none && !current.fading) {
            events += AnimationEvent.Stopped(before.animation)
        }
        if (current.overlayAnimation != before.overlayAnimation) {
            if (before.overlayAnimation != none) events += AnimationEvent.OverlayEnded(before.overlayAnimation)
            if (current.overlayAnimation != none) events += AnimationEvent.OverlayStarted(current.overlayAnimation, current.overlayMask)
        }
        return events
    }

    /** Markers of [animation] with after < time <= upTo. */
    private fun fireMarkers(events: MutableList<AnimationEvent>, animation: Int, after: Float, upTo: Float) {
        for (marker in markers) {
            if (marker.animation == animation && marker.timeSeconds > after && marker.timeSeconds <= upTo) {
                events += AnimationEvent.Marker(animation, marker.name, marker.timeSeconds)
            }
        }
    }
}
