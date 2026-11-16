package com.davemega.hub

import android.util.Log
import com.davemega.hub.avatarforge.AnimationCategory
import com.davemega.hub.avatarforge.AnimationClipInfo
import com.davemega.hub.avatarforge.AnimationEngine
import com.davemega.hub.avatarforge.AnimationSession
import com.davemega.hub.avatarforge.FacialExpression
import com.google.android.filament.RenderableManager
import com.google.android.filament.gltfio.Animator
import com.google.android.filament.gltfio.FilamentAsset

/**
 * Drives a Filament asset from the native `af::anim` animation core.
 *
 * The division of labour is the point. The core owns TIME AND BLENDING -- which clip is playing, where
 * each clock is, how a crossfade is weighted, what the layers and face controller contribute. Filament
 * owns SAMPLING AND SKINNING. This class is only the seam between them, and it contains no animation
 * logic: every decision it applies was made in C++ that the host test suite covers.
 *
 * Concretely, per frame it calls [AnimationSession.advance] and hands the result to gltfio's
 * [Animator] in the order gltfio documents: applyAnimation (current), applyCrossFade (previous),
 * updateBoneMatrices. The alpha it passes is the one `af::anim::CrossFadeCommand` produces, which is
 * Filament's convention (current clip at alpha 1, previous at alpha 0) and is pinned by a host test.
 *
 * Two things this class deliberately does NOT do:
 *
 *  - It does not push the core's bone palette through `RenderableManager.setBonesAsMatrices`. That call
 *    is positional and the core's skin-slot order comes from a different glTF parser than gltfio's. The
 *    Java API offers no way to read gltfio's joint order back and compare, so the correspondence could
 *    not be verified -- and an unverifiable ordering assumption is how a model renders inside-out on one
 *    asset and looks fine on another. Letting gltfio sample its own skeleton keeps the ordering question
 *    inside one parser. The cost is stated in [limitations]: a third blended layer cannot be shown here.
 *
 *  - It does not apply root motion to the asset's transform. `Animator.applyAnimation` already writes
 *    the root bone's own translation channel, so adding the core's accumulated delta on top would move
 *    the character twice. Extracting it instead is not something gltfio exposes. The delta is therefore
 *    reported through [consumeRootMotion] for a character controller or camera to use, which is what a
 *    caller that wants root motion actually needs.
 */
class NativeAnimationDriver(private val renderableManager: RenderableManager) {

    private var session: AnimationSession? = null
    private var animator: Animator? = null

    private var mapping = ClipMapping(IntArray(0), emptyList(), emptyList())
    private var morphOrderProblem: String? = null
    private var morphEntities: IntArray = IntArray(0)
    private var morphInstances: IntArray = IntArray(0)

    /* Reused every frame. The render path allocates nothing. */
    private val frame = AnimationEngine.newFrame()
    private var lastTickNanos = 0L
    private var appliedFrames = 0L

    /** Reasons this driver cannot show everything the core computed. Surfaced, never swallowed. */
    val limitations: List<String>
        get() = buildList {
            mapping.notes.forEach { add(it) }
            morphOrderProblem?.let { add("morph weights are not applied: $it") }
            val layers = session?.layerCount ?: 0
            if (layers > 0) {
                add(
                    "$layers animation layer(s) are active but gltfio can blend only two clips, so the " +
                        "extra layers are computed and not shown. The AvatarForge GLES backend applies them.",
                )
            }
        }

    val isAttached: Boolean get() = session != null && animator != null

    /** The core session, for controls that go beyond transport (layers, face, retargeting). */
    fun session(): AnimationSession? = session

    val clipCount: Int get() = session?.clipCount ?: 0

    /** Clips the core loaded, each annotated with whether this renderer can actually play it. */
    fun playableClips(): List<PlayableClip> {
        val active = session ?: return emptyList()
        return (0 until active.clipCount).mapNotNull { index ->
            val info = active.clipInfo(index) ?: return@mapNotNull null
            val rendererIndex = mapping.rendererIndexFor(index)
            PlayableClip(
                info = info,
                rendererIndex = rendererIndex,
                matchMethod = mapping.methods.getOrElse(index) { ClipMatchMethod.UNMATCHED },
            )
        }
    }

    /**
     * Adopts [glbBytes] -- the same bytes Filament already parsed -- and matches the two clip lists.
     *
     * @return true when the core loaded the asset AND at least one clip can be applied through this
     *   renderer. False leaves the driver detached; [status] explains why. A partial success (some clips
     *   matched, some not) returns true and reports the gaps in [limitations], because refusing to
     *   animate a character whose third clip is unreadable would be worse than animating its first two.
     */
    fun attach(
        target: FilamentAsset,
        targetAnimator: Animator,
        glbBytes: ByteArray,
        source: String,
        licence: String = "",
    ): Boolean {
        detach()
        if (!AnimationEngine.health.usable) {
            status = "native animation core unavailable: ${AnimationEngine.health.problem}"
            return false
        }
        val created = AnimationEngine.create()
        if (created == null) {
            status = "the native animation core could not create a session"
            return false
        }
        if (!created.loadGlb(glbBytes, source, licence)) {
            status = "the animation core refused this asset: ${created.lastError()}"
            val notes = created.loadNotes()
            if (notes.isNotEmpty()) status += " (${notes.first()})"
            created.close()
            return false
        }

        val rendererNames = (0 until targetAnimator.animationCount).map { index ->
            runCatching { targetAnimator.getAnimationName(index) }.getOrNull() ?: ""
        }
        val coreNames = (0 until created.clipCount).map { index ->
            created.clipInfo(index)?.name ?: ""
        }
        mapping = mapClipIndices(coreNames, rendererNames)
        if (mapping.matchedCount == 0) {
            status = "no clip in this asset could be matched to a renderer animation: " +
                mapping.notes.joinToString("; ")
            created.close()
            return false
        }

        session = created
        animator = targetAnimator
        discoverMorphTargets(target, created)

        val notes = created.loadNotes()
        status = buildString {
            append("native animation core driving ${mapping.matchedCount} of ${coreNames.size} clip(s)")
            append(" (${created.jointCount} joints, ${created.rolesMatched}/17 roles resolved")
            if (created.morphCount > 0) append(", ${created.morphCount} morph target(s)")
            append(")")
            if (notes.isNotEmpty()) append("; loader notes: ${notes.joinToString(" | ")}")
        }
        appliedFrames = 0
        lastTickNanos = 0L
        return true
    }

    /**
     * Finds the renderables that actually have morph targets, and checks that the core's target order
     * matches the renderer's. Weights cross the boundary positionally with no names on the wire, so a
     * disagreement would apply every expression to the wrong target -- a face that smiles with its
     * eyelids. Refusing is cheaper than that.
     */
    private fun discoverMorphTargets(target: FilamentAsset, created: AnimationSession) {
        val entities = mutableListOf<Int>()
        val instances = mutableListOf<Int>()
        for (entity in target.entities) {
            val instance = runCatching { renderableManager.getInstance(entity) }.getOrDefault(0)
            if (instance == 0) continue
            val count = runCatching { renderableManager.getMorphTargetCount(instance) }.getOrDefault(0)
            if (count > 0) {
                entities += entity
                instances += instance
            }
        }
        morphEntities = entities.toIntArray()
        morphInstances = instances.toIntArray()
        morphOrderProblem = null

        if (morphEntities.isEmpty()) {
            if (created.morphCount > 0) {
                morphOrderProblem = "the core loaded ${created.morphCount} morph target(s) but the " +
                    "renderer built no morphing renderable for this asset"
            }
            return
        }
        val rendererNames = runCatching { target.getMorphTargetNames(morphEntities[0]) }
            .getOrNull()?.toList() ?: emptyList()
        morphOrderProblem = verifyMorphTargetOrder(created.morphNames(), rendererNames)
    }

    /**
     * Advances the core and applies the result. Call from the Choreographer callback.
     *
     * @param frameTimeNanos the frame timestamp Choreographer supplied, used to derive a delta so this
     *   class holds no clock of its own and cannot drift from the renderer's.
     * @return true when something was applied to the renderer.
     */
    fun tick(frameTimeNanos: Long): Boolean {
        val active = session ?: return false
        val activeAnimator = animator ?: return false
        if (active.isClosed) return false

        val deltaSeconds = if (lastTickNanos == 0L) {
            0f
        } else {
            ((frameTimeNanos - lastTickNanos).coerceAtLeast(0L) / 1_000_000_000f)
        }
        lastTickNanos = frameTimeNanos

        val morphsWritten = active.advance(deltaSeconds, frame)

        val rendererA = mapping.rendererIndexFor(frame.clipA)
        if (rendererA < 0) {
            /* Nothing to apply. This is not an error to retry every frame: it means the core is idle or
               the current clip has no renderer counterpart, which limitations already reports. */
            return false
        }
        runCatching { activeAnimator.applyAnimation(rendererA, frame.timeA) }.onFailure {
            status = "applyAnimation($rendererA, ${frame.timeA}) failed: ${it.message}"
            return false
        }

        if (frame.isBlending) {
            val rendererB = mapping.rendererIndexFor(frame.clipB)
            if (rendererB >= 0) {
                /* gltfio's documented order: applyAnimation, then applyCrossFade, then updateBoneMatrices.
                   alpha is the CURRENT clip's weight -- see CrossFadeCommand. */
                runCatching {
                    activeAnimator.applyCrossFade(rendererB, frame.timeB, frame.alpha)
                }.onFailure {
                    status = "applyCrossFade($rendererB, ${frame.timeB}, ${frame.alpha}) failed: ${it.message}"
                }
            }
        }
        runCatching { activeAnimator.updateBoneMatrices() }.onFailure {
            status = "updateBoneMatrices failed: ${it.message}"
            return false
        }

        if (morphsWritten > 0 && morphOrderProblem == null && morphInstances.isNotEmpty()) {
            val weights = active.morphWeights
            for (instance in morphInstances) {
                runCatching { renderableManager.setMorphWeights(instance, weights, 0) }.onFailure {
                    morphOrderProblem = "setMorphWeights failed: ${it.message}"
                }
            }
        }

        appliedFrames++
        return true
    }

    /** Events the core fired since the previous call -- footsteps, contacts, whatever a clip declared. */
    fun drainEvents(): List<String> = session?.drainEvents() ?: emptyList()

    /**
     * The root-motion delta accumulated since the previous call, and zeroes the accumulator.
     *
     * Reported, not applied -- see the class comment for why applying it here would move the character
     * twice. A character controller or a follow camera consumes this; the renderer does not.
     */
    fun consumeRootMotion(): FloatArray = session?.consumeRootMotion() ?: floatArrayOf(0f, 0f, 0f)

    /** Microseconds the core spent on the most recent advance, for the on-screen performance readout. */
    fun advanceMicros(): Float = frame.advanceMicros

    fun framesApplied(): Long = appliedFrames

    /* ------------------------------------------------------------------ controls ------------- */
    /* Thin forwards. Each returns the core's own answer rather than a guess, so a refusal reaches the
       UI: a button that silently does nothing is indistinguishable from a bug. */

    fun play(nativeClipIndex: Int): Boolean {
        val active = session ?: return false
        if (!active.play(nativeClipIndex)) {
            status = active.lastError()
            return false
        }
        return true
    }

    fun crossfadeTo(nativeClipIndex: Int, seconds: Float): Boolean {
        val active = session ?: return false
        if (!active.crossfadeTo(nativeClipIndex, seconds)) {
            status = active.lastError()
            return false
        }
        return true
    }

    fun pause() = session?.pause() ?: Unit
    fun resume() = session?.resume() ?: Unit
    fun stop() = session?.stop() ?: Unit
    fun seek(seconds: Float) = session?.seek(seconds) ?: Unit
    fun setSpeed(speed: Float): Boolean = session?.setSpeed(speed) ?: false
    fun setLoop(loop: Boolean) = session?.setLoop(loop) ?: Unit

    fun setBlinkEnabled(enabled: Boolean) = session?.setBlinkEnabled(enabled) ?: Unit
    fun setExpression(expression: FacialExpression, amount: Float): Int =
        session?.setExpression(expression, amount) ?: -1

    /**
     * Starts the approximate, spelling-based viseme timeline for [text].
     *
     * Returns the number of viseme keys produced. 0 means nothing will move, and any UI offering this
     * must describe it as an approximation -- it is not a phoneme recogniser and must not be presented
     * as one.
     */
    fun speak(text: String, totalSeconds: Float): Int {
        val keys = session?.speak(text, totalSeconds) ?: -1
        if (keys == 0) status = session?.lastError() ?: "no viseme keys were produced"
        if (keys > 0 && morphOrderProblem != null) {
            status = "lip-sync is running but its weights are not reaching the renderer: $morphOrderProblem"
        }
        return keys
    }

    fun stopSpeaking() = session?.stopSpeaking() ?: Unit

    /** Clips grouped by category, so the picker shows only categories this character actually has. */
    fun clipsByCategory(): Map<AnimationCategory, List<PlayableClip>> =
        playableClips().groupBy { it.info.category }

    /** Releases the core session. Safe to call twice and safe to call while detached. */
    fun detach() {
        session?.close()
        session = null
        animator = null
        mapping = ClipMapping(IntArray(0), emptyList(), emptyList())
        morphEntities = IntArray(0)
        morphInstances = IntArray(0)
        morphOrderProblem = null
        appliedFrames = 0
        lastTickNanos = 0L
    }

    /** The last thing worth telling the user, including refusals and partial successes. */
    var status: String = "not attached"
        private set

    private companion object {
        const val TAG = "NativeAnimationDriver"
    }

    init {
        /* A driver with no core attached must never be mistaken for a working one. */
        if (!AnimationEngine.health.usable) {
            status = "native animation core unavailable: ${AnimationEngine.health.problem}"
            Log.w(TAG, status)
        }
    }
}

/** One core clip plus whether this renderer can apply it, and on what evidence. */
data class PlayableClip(
    val info: AnimationClipInfo,
    /** The renderer's animation index, or -1 when this clip cannot be applied here. */
    val rendererIndex: Int,
    val matchMethod: ClipMatchMethod,
) {
    val playable: Boolean get() = rendererIndex >= 0
}
