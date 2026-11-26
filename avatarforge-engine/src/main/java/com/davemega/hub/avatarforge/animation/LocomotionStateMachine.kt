package com.davemega.hub.avatarforge.animation

/**
 * Stand, idle, walk and run as one control: a pace from 0 to 1 picks the state and the state
 * machine crossfades between them, so a caller never names a clip. One-shot actions (a jump, a
 * punch) take over the body and hand it back; gestures (a wave) play on the upper body while the
 * legs go on.
 *
 * It only decides. Everything it does goes through an [AnimationController], so it runs against
 * the renderer on a phone and against a recording stand-in in a unit test.
 */
class LocomotionStateMachine(
    private val library: AnimationLibrary,
    private val controller: AnimationController,
    private val fadeSeconds: Float = 0.3f,
    private val onStateChanged: (from: State, to: State) -> Unit = { _, _ -> }
) {
    enum class State { STAND, IDLE, WALK, RUN, ACTION }

    private val idle = library.firstOf("idle", "breathe")
    private val walk = library.firstOf("walk")
    private val run = library.firstOf("run", "jog")

    /** False when this model has no idle, walk and run to move between (a file from elsewhere). */
    val available: Boolean get() = idle != null && walk != null && run != null

    var state: State = State.STAND
        private set

    /** The pace last asked for, 0..1. */
    var pace: Float = 0f
        private set

    private var action: AnimationEntry? = null
    private var actionSeen = false

    /**
     * 0 stands idle, up to [RUN_AT] walks, above it runs. Inside a state the clip is played
     * faster or slower with the pace. The thresholds have a gap between going up and coming
     * down, so a pace that hovers at a threshold does not flip the state every frame.
     */
    fun setPace(value: Float): Boolean {
        if (!available) return false
        pace = if (value.isFinite()) value.coerceIn(0f, 1f) else 0f
        if (state == State.ACTION) return true          /* taken up again when the action ends */
        val target = stateFor(pace, state)
        if (target != state) enter(target) else controller.setAnimationSpeed(playbackSpeed(state, pace))
        return true
    }

    /** Plays [name] once on the whole body, then returns to what the pace asks for. */
    fun trigger(name: String): Boolean {
        val entry = library.find(name) ?: return false
        action = entry
        actionSeen = false
        val from = state
        state = State.ACTION
        controller.crossFadeTo(entry.index, fadeSeconds.coerceAtMost(0.2f), loop = false, speed = 1f)
        if (from != State.ACTION) onStateChanged(from, State.ACTION)
        return true
    }

    /** Plays [name] once on part of the body, over whatever the legs are doing. */
    fun gesture(name: String, mask: JointMask = JointMask.UPPER_BODY, weight: Float = 1f): Boolean {
        val entry = library.find(name) ?: return false
        controller.setOverlay(entry.index, mask, weight, loop = false, fadeSeconds = 0.2f, speed = 1f)
        return true
    }

    /** Back to the rest pose. */
    fun stop() {
        action = null
        controller.clearOverlay(fadeSeconds)
        if (state != State.STAND) {
            val from = state
            state = State.STAND
            pace = 0f
            controller.crossFadeTo(AnimationPlaybackState.NONE, fadeSeconds)
            onStateChanged(from, State.STAND)
        }
    }

    /** Call with the renderer's state, once a frame or so: it ends an action when its clip has. */
    fun onPlayback(playback: AnimationPlaybackState) {
        val running = action ?: return
        if (state != State.ACTION) return
        if (playback.animation == running.index) {
            actionSeen = true
            if (!playback.finished) return
        } else if (!actionSeen) {
            return                                       /* the renderer has not started it yet */
        }
        action = null
        val target = if (available) stateFor(pace, State.IDLE) else State.STAND
        enter(target, from = State.ACTION)
    }

    private fun enter(target: State, from: State = state) {
        state = target
        when (target) {
            State.IDLE -> controller.crossFadeTo(idle!!.index, fadeSeconds, loop = true, speed = 1f)
            State.WALK -> controller.crossFadeTo(walk!!.index, fadeSeconds, loop = true, speed = playbackSpeed(State.WALK, pace))
            State.RUN -> controller.crossFadeTo(run!!.index, fadeSeconds, loop = true, speed = playbackSpeed(State.RUN, pace))
            State.STAND -> controller.crossFadeTo(AnimationPlaybackState.NONE, fadeSeconds)
            State.ACTION -> Unit
        }
        if (from != target) onStateChanged(from, target)
    }

    companion object {
        const val WALK_AT = 0.10f
        const val RUN_AT = 0.60f
        private const val GAP = 0.05f

        /** The state a pace asks for, given the state it is in (the thresholds differ by direction). */
        fun stateFor(pace: Float, current: State): State {
            val walkUp = WALK_AT
            val walkDown = WALK_AT - GAP
            val runUp = RUN_AT
            val runDown = RUN_AT - GAP
            return when (current) {
                State.RUN -> if (pace >= runDown) State.RUN else if (pace >= walkDown) State.WALK else State.IDLE
                State.WALK -> if (pace >= runUp) State.RUN else if (pace >= walkDown) State.WALK else State.IDLE
                else -> if (pace >= runUp) State.RUN else if (pace >= walkUp) State.WALK else State.IDLE
            }
        }

        /** How fast the state's clip plays at a pace: a slow walk to a brisk one, a jog to a sprint. */
        fun playbackSpeed(state: State, pace: Float): Float = when (state) {
            State.WALK -> 0.75f + 0.55f * ((pace - WALK_AT) / (RUN_AT - WALK_AT)).coerceIn(0f, 1f)
            State.RUN -> 0.85f + 0.35f * ((pace - RUN_AT) / (1f - RUN_AT)).coerceIn(0f, 1f)
            else -> 1f
        }
    }
}
