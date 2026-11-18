package com.davemega.hub.avatarforge

/**
 * Kotlin surface over the native `af::anim` animation core.
 *
 * This file contains no animation logic. Every method forwards to the C++ core through JNI, which is
 * the same core the host test suite (`animation_system_test.cpp`, 310 checks) exercises and the same
 * core the AvatarForge GLES backend uses. A second state machine, viseme table or blender written here
 * would be a second animation engine, and the two would drift apart the first time either changed.
 *
 * What this layer does own is lifetime and allocation discipline:
 *  - a session is an [AutoCloseable] handle; [AnimationSession.close] invalidates it natively, and the
 *    native side checks a magic word before every dereference, so a use-after-close returns an error
 *    instead of crashing the render thread;
 *  - [AnimationSession.advance] writes into buffers the session allocated once. Nothing on the
 *    per-frame path allocates a Java object, because this runs 60 times a second inside a Choreographer
 *    callback and a GC pause there is a dropped frame the user sees.
 *
 * Threading: a session is NOT internally synchronised. Confine one session to one thread -- in this app
 * that is the render thread, the same ownership rule the Filament [com.google.android.filament.Engine]
 * itself uses. Passing a session between threads needs external locking.
 */

/**
 * Animation library categories, in the order the native `af::anim::Category` enum declares them.
 *
 * The ordinals are ABI: [nativeId] is what crosses JNI, so reordering this enum silently mislabels
 * every clip in the picker. [ALL] is the search filter meaning "no category restriction" and maps to
 * the native `Category::COUNT` sentinel, which is why it is -1 here and `CATEGORY_ALL` on the wire.
 */
enum class AnimationCategory(val nativeId: Int) {
    UNCATEGORIZED(0),
    IDLE(1),
    WALKING(2),
    RUNNING(3),
    JUMPING(4),
    DANCING(5),
    COMBAT(6),
    GESTURES(7),
    EMOTIONS(8),
    TALKING(9),
    FACIAL_EXPRESSIONS(10);

    companion object {
        /** Wire value for "every category". */
        const val CATEGORY_ALL: Int = -1

        fun fromNativeId(id: Int): AnimationCategory =
            entries.firstOrNull { it.nativeId == id } ?: UNCATEGORIZED
    }
}

/** Facial expression presets, in the order the native `af::anim::Expression` enum declares them. */
enum class FacialExpression(val nativeId: Int) {
    NEUTRAL(0),
    HAPPY(1),
    SAD(2),
    ANGRY(3),
    SURPRISED(4),
    FEAR(5),
    DISGUST(6);

    companion object {
        fun fromNativeId(id: Int): FacialExpression =
            entries.firstOrNull { it.nativeId == id } ?: NEUTRAL
    }
}

/** Which joints a layer is allowed to drive. Resolved natively from this character's own roles. */
enum class BoneMaskPreset(val nativeId: Int) {
    /** Arms, spine, neck and head -- everything above the hips. */
    UPPER_BODY(0),
    /** Hips and legs. */
    LOWER_BODY(1),
    /** Every joint. */
    WHOLE_BODY(2),
}

/** One clip the session can play. */
data class AnimationClipInfo(
    val index: Int,
    val name: String,
    val durationSeconds: Float,
    val looping: Boolean,
    val category: AnimationCategory,
    val channelCount: Int,
    /** True when the clip translates the joint that carries the character, so root motion is meaningful. */
    val hasRootMotion: Boolean,
)

/** One searchable entry in the animation library, with its provenance and licence for attribution. */
data class AnimationLibraryItem(
    val name: String,
    val category: AnimationCategory,
    val durationSeconds: Float,
    val looping: Boolean,
    /** True for the engine's own hand-authored clips, false for clips read out of an imported file. */
    val authored: Boolean,
    val source: String,
    val licence: String,
    /** Index into the owning session's clips, or -1 when this entry is not bound to a loaded player. */
    val clipIndex: Int,
)

/** Why two characters are or are not retarget-compatible. Shown to the user verbatim. */
data class RetargetCompatibility(
    val compatible: Boolean,
    val roleCoverage: Float,
    val translationScale: Float,
    val mappedJoints: Int,
    val destinationJoints: Int,
    val notes: List<String>,
    val missingRoles: List<String>,
)

/** Per-frame measurement, read back for the on-screen performance panel. */
data class AnimationPerformance(
    val lastAdvanceMicros: Double,
    val averageAdvanceMicros: Double,
    val maxAdvanceMicros: Double,
    val frames: Long,
    val posesSampled: Long,
    val jointCount: Int,
    val channelCount: Int,
)

/**
 * What the renderer must apply this frame. Mutable and reused on purpose: [AnimationSession.advance]
 * fills a caller-owned instance so the 60 Hz path allocates nothing.
 *
 * The five blend fields are the contract with a GPU renderer's crossfade primitive. Applied literally:
 *
 * ```
 * animator.applyAnimation(clipA, timeA)
 * if (isBlending) animator.applyCrossFade(clipB, timeB, alpha)
 * animator.updateBoneMatrices()
 * ```
 *
 * [alpha] is in FILAMENT's convention, which gltfio documents and which is easy to get backwards: the
 * animation passed to `applyAnimation` is the CURRENT one and corresponds to alpha = 1, while the
 * previous animation corresponds to alpha = 0. So [alpha] is the weight of [clipA], rising from 0 to 1
 * across the fade. Inverting it does not crash anything -- it plays every transition in reverse,
 * fading OUT of the clip the user asked for. The native `af::anim::CrossFadeCommand` that produces it
 * is pinned by the host test suite, so the convention is verified rather than assumed.
 *
 * When [alpha] is 1 there is no blend in progress and [clipB] is -1.
 */
class AnimationFrame internal constructor() {
    var clipA: Int = -1
    var timeA: Float = 0f
    /** The clip fading OUT, or -1 when [alpha] is 1. Its own index into the same clip list as [clipA]. */
    var clipB: Int = -1
    /** [clipB]'s OWN clock. Not derived from [timeA]: the two clips run independently and usually
     *  differ in duration, so a fade source reconstructed from the destination's time is wrong. */
    var timeB: Float = 0f
    /** Weight of [clipA]: 1 means [clipA] alone, 0 means [clipB] alone. Filament's convention. */
    var alpha: Float = 1f
    var rootMotionX: Float = 0f
    var rootMotionY: Float = 0f
    var rootMotionZ: Float = 0f
    var advanceMicros: Float = 0f
    var averageAdvanceMicros: Float = 0f
    /** How many morph weights [AnimationSession.morphWeights] holds after this call. */
    var morphWeightsWritten: Int = 0

    /** True while two clips are being blended and the renderer must call `applyCrossFade`. */
    val isBlending: Boolean get() = clipB >= 0 && alpha < 1f

    internal fun reset() {
        clipA = -1; timeA = 0f; clipB = -1; timeB = 0f; alpha = 1f
        rootMotionX = 0f; rootMotionY = 0f; rootMotionZ = 0f
        advanceMicros = 0f; averageAdvanceMicros = 0f; morphWeightsWritten = 0
    }
}

/** Reported by [AnimationEngine.health] so the UI can say why animation is unavailable. */
data class AnimationHealth(
    val nativeLibraryLoaded: Boolean,
    val problem: String?,
) {
    val usable: Boolean get() = nativeLibraryLoaded && problem == null
}

/**
 * One animation session: a skeleton, its clips, layers, face controller and clocks, all native.
 *
 * Obtain one from [AnimationEngine.create] and [close] it when the character goes away. Every method
 * is safe to call on a closed session -- it reports failure rather than crashing -- but a closed
 * session animates nothing, so leaking one leaks a skeleton and its clips.
 */
class AnimationSession internal constructor(internal val handle: Long) : AutoCloseable {

    @Volatile
    private var closed = false

    /* Reused per-frame buffers. Allocated once, never on the render path. */
    private val frameBuffer = FloatArray(FRAME_FLOATS)
    private var morphBuffer = FloatArray(0)
    private var paletteBuffer = FloatArray(0)

    /** Morph weights written by the most recent [advance], in [morphNames] order. */
    val morphWeights: FloatArray get() = morphBuffer

    val isClosed: Boolean get() = closed || !AnimationNative.nativeIsAlive(handle)

    /* ------------------------------------------------------------------ loading -------------- */

    /**
     * Imports an animated GLB and adopts its skeleton, clips and morph targets.
     *
     * @return true when the asset loaded and yielded at least one playable clip or morph target.
     *   False means nothing was adopted; [lastError] says why. A file with no animation is refused
     *   rather than silently playing as a static mesh, because "loaded" that animates nothing is the
     *   failure mode this API exists to avoid.
     */
    fun loadGlb(bytes: ByteArray, source: String, licence: String = ""): Boolean {
        if (isClosed) return false
        val rc = AnimationNative.nativeLoadGlb(handle, bytes, source, licence)
        if (rc != 0 && morphCount == 0) return false
        if (morphBuffer.size != morphCount) morphBuffer = FloatArray(morphCount)
        return rc == 0
    }

    /**
     * Imports the same asset from a path, with the identical contract to [loadGlb].
     *
     * Prefer this whenever the asset is a real file. [loadGlb] needs the whole file as a Java
     * ByteArray, and the JNI then copies that array into native memory before parsing, so a generated
     * avatar of 79.77 MiB occupies that much twice over - inside a Dalvik heap the CI emulator caps at
     * a 192 MiB growth limit. Reading the path in C++ avoids both the heap allocation and the copy.
     */
    fun loadGlbFile(path: String, source: String, licence: String = ""): Boolean {
        if (isClosed) return false
        val rc = AnimationNative.nativeLoadGlbFile(handle, path, source, licence)
        if (rc != 0 && morphCount == 0) return false
        if (morphBuffer.size != morphCount) morphBuffer = FloatArray(morphCount)
        return rc == 0
    }

    /** Adds the engine's 16 hand-authored clips to the library index. Returns the new entry count. */
    fun indexAuthoredLibrary(): Int =
        if (isClosed) 0 else AnimationNative.nativeLoadAuthoredLibrary(handle)

    /** What the loader decided, including every channel it refused and why. Never empty on failure. */
    fun loadNotes(): List<String> =
        if (isClosed) emptyList() else AnimationNative.nativeLoadNotes(handle).toList()

    fun lastError(): String = if (closed) "session is closed" else AnimationNative.nativeLastError(handle)

    /* ------------------------------------------------------------ introspection -------------- */

    val clipCount: Int get() = if (isClosed) 0 else AnimationNative.nativeClipCount(handle)
    val jointCount: Int get() = if (isClosed) 0 else AnimationNative.nativeJointCount(handle)
    /** Joints the skin actually addresses; the closure can be larger because it carries ancestors. */
    val skinSlotCount: Int get() = if (isClosed) 0 else AnimationNative.nativeSkinSlotCount(handle)
    /** How many of the 17 humanoid roles this skeleton resolved. 17 means fully retargetable. */
    val rolesMatched: Int get() = if (isClosed) 0 else AnimationNative.nativeRolesMatched(handle)
    val morphCount: Int get() = if (isClosed) 0 else AnimationNative.nativeMorphCount(handle)

    fun clipIndex(name: String): Int =
        if (isClosed) -1 else AnimationNative.nativeClipIndex(handle, name)

    fun jointName(index: Int): String? =
        if (isClosed) null else AnimationNative.nativeJointName(handle, index)

    fun morphName(index: Int): String? =
        if (isClosed) null else AnimationNative.nativeMorphName(handle, index)

    fun morphNames(): List<String> =
        (0 until morphCount).mapNotNull { morphName(it) }

    fun clipInfo(index: Int): AnimationClipInfo? {
        if (isClosed) return null
        val name = AnimationNative.nativeClipName(handle, index) ?: return null
        return AnimationClipInfo(
            index = index,
            name = name,
            durationSeconds = AnimationNative.nativeClipDuration(handle, index),
            looping = AnimationNative.nativeClipLooping(handle, index),
            category = AnimationCategory.fromNativeId(AnimationNative.nativeClipCategory(handle, index)),
            channelCount = AnimationNative.nativeClipChannelCount(handle, index),
            hasRootMotion = AnimationNative.nativeClipHasRootMotion(handle, index),
        )
    }

    fun clips(): List<AnimationClipInfo> = (0 until clipCount).mapNotNull { clipInfo(it) }

    /* ------------------------------------------------------------------ transport ------------ */

    fun play(index: Int): Boolean = !isClosed && AnimationNative.nativePlayIndex(handle, index)

    fun play(name: String): Boolean = !isClosed && AnimationNative.nativePlay(handle, name)

    fun pause() { if (!isClosed) AnimationNative.nativePause(handle) }

    fun resume() { if (!isClosed) AnimationNative.nativeResume(handle) }

    /** Stops and returns the timeline to 0. */
    fun stop() { if (!isClosed) AnimationNative.nativeStop(handle) }

    /** Timeline scrubbing. Clamped to [0, duration] natively. */
    fun seek(seconds: Float) { if (!isClosed) AnimationNative.nativeSeek(handle, seconds) }

    /**
     * Playback speed. A negative or non-finite speed is REFUSED natively and the player keeps its
     * previous value, so the return value tells the caller whether the change took effect.
     */
    fun setSpeed(speed: Float): Boolean = !isClosed && AnimationNative.nativeSetSpeed(handle, speed)

    val speed: Float get() = if (isClosed) 0f else AnimationNative.nativeSpeed(handle)

    fun setLoop(loop: Boolean) { if (!isClosed) AnimationNative.nativeSetLoop(handle, loop) }

    val position: Float get() = if (isClosed) 0f else AnimationNative.nativePosition(handle)

    val currentDuration: Float get() = if (isClosed) 0f else AnimationNative.nativeCurrentDuration(handle)

    val isPlaying: Boolean get() = !isClosed && AnimationNative.nativeIsPlaying(handle)

    val currentClip: Int get() = if (isClosed) -1 else AnimationNative.nativeCurrentClip(handle)

    /* ------------------------------------------------------------------ blending ------------- */

    /** Begins a crossfade to another clip over [seconds]. False when there is no such clip. */
    fun crossfadeTo(index: Int, seconds: Float): Boolean =
        !isClosed && AnimationNative.nativeCrossfadeToIndex(handle, index, seconds)

    fun crossfadeTo(name: String, seconds: Float): Boolean =
        !isClosed && AnimationNative.nativeCrossfadeTo(handle, name, seconds)

    fun setCrossfadeEnabled(enabled: Boolean) {
        if (!isClosed) AnimationNative.nativeSetCrossfadeEnabled(handle, enabled)
    }

    val isTransitioning: Boolean get() = !isClosed && AnimationNative.nativeIsTransitioning(handle)

    /** 0 at the start of a crossfade, 1 when it has finished. 1 when no transition is running. */
    val transitionProgress: Float
        get() = if (isClosed) 1f else AnimationNative.nativeTransitionProgress(handle)

    /**
     * The blend weight of the INCOMING clip, in the convention [AnimationFrame.alpha] documents.
     * Provided separately from [transitionProgress] because the two mean different things even though
     * they currently share a value: progress is how far along a transition is, alpha is how much of a
     * clip to draw. Keeping them apart means a future easing curve cannot silently change what a
     * renderer blends.
     */
    val crossfadeAlpha: Float
        get() = if (isClosed) 1f else AnimationNative.nativeCrossfadeAlpha(handle)

    /**
     * Builds one state per loaded clip. Returns the state count, or -1 when there are no clips.
     * @param allToAll when true, declares every state reachable from every other one; when false the
     *   caller declares edges with [addTransition] and any transition not declared is REFUSED.
     */
    fun buildStateMachine(crossfadeSeconds: Float, allToAll: Boolean = true): Int =
        if (isClosed) -1 else AnimationNative.nativeBuildStateMachine(handle, crossfadeSeconds, allToAll)

    fun addTransition(from: String, to: String, crossfadeSeconds: Float): Boolean =
        !isClosed && AnimationNative.nativeAddTransition(handle, from, to, crossfadeSeconds)

    /** Hands the clocks to the state machine. While driven, [play] and [seek] no longer own time. */
    fun setStateMachineDriven(driven: Boolean) {
        if (!isClosed) AnimationNative.nativeSetStateMachineDriven(handle, driven)
    }

    /**
     * Requests a state change. Returns false -- and changes nothing -- when the state is unknown or no
     * transition to it was declared. That refusal is deliberate: an undeclared edge snapping instantly
     * is how a state machine ends up looking broken, and [lastError] carries the reason for the UI.
     */
    fun requestState(name: String): Boolean =
        !isClosed && AnimationNative.nativeRequestState(handle, name)

    val currentStateName: String?
        get() = if (isClosed) null else AnimationNative.nativeCurrentStateName(handle)

    /* ------------------------------------------------------------------- layers -------------- */

    /** Adds an animation layer. Returns its index, or -1 when the clip does not exist. */
    fun addLayer(clipIndex: Int, weight: Float, additive: Boolean = false): Int =
        if (isClosed) -1 else AnimationNative.nativeAddLayer(handle, clipIndex, weight, additive)

    val layerCount: Int get() = if (isClosed) 0 else AnimationNative.nativeLayerCount(handle)

    fun setLayerClip(layer: Int, clipIndex: Int): Boolean =
        !isClosed && AnimationNative.nativeSetLayerClip(handle, layer, clipIndex)

    fun setLayerWeight(layer: Int, weight: Float): Boolean =
        !isClosed && AnimationNative.nativeSetLayerWeight(handle, layer, weight)

    fun setLayerEnabled(layer: Int, enabled: Boolean): Boolean =
        !isClosed && AnimationNative.nativeSetLayerEnabled(handle, layer, enabled)

    fun setLayerAdditive(layer: Int, additive: Boolean): Boolean =
        !isClosed && AnimationNative.nativeSetLayerAdditive(handle, layer, additive)

    /**
     * Restricts a layer to part of the body. Returns how many joints the mask includes, or -1 on
     * failure. The mask is built from THIS skeleton's roles, so "upper body" means this character's
     * arms, spine and head rather than a hardcoded range of joint indices.
     */
    fun setLayerMask(layer: Int, preset: BoneMaskPreset): Int =
        if (isClosed) -1 else AnimationNative.nativeSetLayerMask(handle, layer, preset.nativeId)

    /** Registers an event on a clip's timeline. Fires once per crossing, reported by [drainEvents]. */
    fun addEvent(clipIndex: Int, timeSeconds: Float, name: String, value: Float = 0f): Boolean =
        !isClosed && AnimationNative.nativeAddEvent(handle, clipIndex, timeSeconds, name, value)

    /* --------------------------------------------------------------- face / lip-sync --------- */

    /**
     * Applies an expression preset. Returns how many morph channels it actually wrote -- 0 on a
     * character with no ARKit-named targets, which is the honest answer rather than a silent no-op.
     */
    fun setExpression(expression: FacialExpression, amount: Float): Int =
        if (isClosed) -1 else AnimationNative.nativeSetExpression(handle, expression.nativeId, amount)

    /** Sets one blendshape by name. False when this character has no target with that name. */
    fun setMorphWeight(name: String, value: Float): Boolean =
        !isClosed && AnimationNative.nativeSetMorphWeight(handle, name, value)

    /** Sets one blendshape by index, for assets that ship morph targets without names. */
    fun setMorphWeightAt(index: Int, value: Float): Boolean =
        !isClosed && AnimationNative.nativeSetMorphWeightAt(handle, index, value)

    fun morphWeightAt(index: Int): Float =
        if (isClosed) 0f else AnimationNative.nativeMorphWeightAt(handle, index)

    fun resetFace() { if (!isClosed) AnimationNative.nativeResetFace(handle) }

    /** Autonomous blinking. Off by default: nothing should move a face the user did not ask about. */
    fun setBlinkEnabled(enabled: Boolean) {
        if (!isClosed) AnimationNative.nativeSetBlinkEnabled(handle, enabled)
    }

    fun setBlinkInterval(minSeconds: Float, maxSeconds: Float) {
        if (!isClosed) AnimationNative.nativeSetBlinkInterval(handle, minSeconds, maxSeconds)
    }

    fun triggerBlink() { if (!isClosed) AnimationNative.nativeTriggerBlink(handle) }

    /**
     * Builds an approximate viseme timeline for [text] and starts driving it.
     *
     * This is a spelling-based approximation, not a phoneme recogniser, and the UI must say so
     * wherever it is offered. Returns the number of viseme keys produced; 0 means the utterance was
     * empty or the duration was not positive, so nothing will move.
     */
    fun speak(text: String, totalSeconds: Float): Int =
        if (isClosed) -1 else AnimationNative.nativeSpeak(handle, text, totalSeconds)

    fun stopSpeaking() { if (!isClosed) AnimationNative.nativeStopSpeaking(handle) }

    val isSpeaking: Boolean get() = !isClosed && AnimationNative.nativeIsSpeaking(handle)

    /** Channels the face controller was asked for but this asset does not have. Diagnostics, not noise. */
    fun unboundFaceChannels(): List<String> =
        if (isClosed) emptyList() else AnimationNative.nativeUnboundFaceChannels(handle).toList()

    /* --------------------------------------------------------------- retargeting ------------- */

    /** Whether [source]'s motion can be retargeted onto this session's skeleton, and why or why not. */
    fun checkCompatibility(source: AnimationSession): RetargetCompatibility {
        if (isClosed || source.isClosed) {
            return RetargetCompatibility(false, 0f, 0f, 0, 0, emptyList(), emptyList())
        }
        /* Notes and missing roles come back through the event queue, which the native side already
           owns; draining it here keeps this call allocation-light and avoids a second JNI array. */
        drainEvents()
        val values = AnimationNative.nativeCheckCompatibility(handle, source.handle)
        val drained = drainEvents()
        return RetargetCompatibility(
            compatible = values[0] > 0.5f,
            roleCoverage = values[1],
            translationScale = values[2],
            mappedJoints = values[3].toInt(),
            destinationJoints = values[4].toInt(),
            notes = drained.filter { it.startsWith("note:") }.map { it.removePrefix("note:") },
            missingRoles = drained.filter { it.startsWith("missing:") }.map { it.removePrefix("missing:") },
        )
    }

    /**
     * Bakes [source]'s clip onto this skeleton by resampling it through the role map, and adds the
     * result as a new clip. Returns the new clip index, or -1 when the skeletons are not compatible --
     * in which case [lastError] explains which roles are missing. Nothing is invented: an unmapped
     * destination joint holds its rest pose and is reported.
     */
    fun retargetFrom(source: AnimationSession, sourceClipIndex: Int, newName: String = ""): Int =
        if (isClosed || source.isClosed) -1
        else AnimationNative.nativeRetarget(handle, source.handle, sourceClipIndex, newName)

    /* ----------------------------------------------------------------- per frame ------------- */

    /**
     * Advances every clock and fills [into] with what the renderer must apply.
     *
     * @param deltaSeconds wall time since the previous call. Negative, NaN and absurd values are
     *   sanitised natively rather than trusted.
     * @return the number of morph weights written into [morphWeights].
     */
    fun advance(deltaSeconds: Float, into: AnimationFrame): Int {
        if (isClosed) {
            into.reset()
            return 0
        }
        val written = AnimationNative.nativeAdvance(handle, deltaSeconds, frameBuffer, morphBuffer)
        if (written < 0) {
            into.reset()
            return 0
        }
        into.clipA = frameBuffer[0].toInt()
        into.timeA = frameBuffer[1]
        into.clipB = frameBuffer[2].toInt()
        into.timeB = frameBuffer[3]
        into.alpha = frameBuffer[4]
        into.rootMotionX = frameBuffer[5]
        into.rootMotionY = frameBuffer[6]
        into.rootMotionZ = frameBuffer[7]
        into.advanceMicros = frameBuffer[8]
        into.averageAdvanceMicros = frameBuffer[9]
        into.morphWeightsWritten = written
        return written
    }

    /** Takes the accumulated root-motion delta and zeroes the accumulator. */
    fun consumeRootMotion(): FloatArray =
        if (isClosed) floatArrayOf(0f, 0f, 0f) else AnimationNative.nativeConsumeRootMotion(handle)

    fun setRootMotionEnabled(enabled: Boolean) {
        if (!isClosed) AnimationNative.nativeSetRootMotionEnabled(handle, enabled)
    }

    /**
     * The bone palette for this frame: 16 column-major floats per skin slot, exactly what
     * `RenderableManager.setBonesAsMatrices` consumes. Returns the number of matrices written.
     */
    fun bonePalette(out: FloatArray): Int {
        if (isClosed) return -1
        if (paletteBuffer.size != out.size) paletteBuffer = out
        return AnimationNative.nativeBonePalette(handle, out)
    }

    /** Allocates a palette buffer sized for this skeleton (16 floats per skin slot). */
    fun allocatePaletteBuffer(): FloatArray = FloatArray(skinSlotCount.coerceAtLeast(0) * 16)

    fun performance(): AnimationPerformance {
        if (isClosed) return AnimationPerformance(0.0, 0.0, 0.0, 0, 0, 0, 0)
        val v = AnimationNative.nativeStats(handle)
        return AnimationPerformance(
            lastAdvanceMicros = v[0],
            averageAdvanceMicros = v[1],
            maxAdvanceMicros = v[2],
            frames = v[3].toLong(),
            posesSampled = v[4].toLong(),
            jointCount = v[5].toInt(),
            channelCount = v[6].toInt(),
        )
    }

    fun resetPerformance() { if (!isClosed) AnimationNative.nativeResetStats(handle) }

    /* ------------------------------------------------------------------ events --------------- */

    /**
     * Events fired since the previous call, as `"name"` / `"value"` pairs flattened into one list.
     * Empty on a frame with no events, which is the normal case -- nothing is allocated for silence.
     */
    fun drainEvents(): List<String> =
        if (isClosed) emptyList() else AnimationNative.nativeDrainEvents(handle).toList()

    /* ----------------------------------------------------------------- library --------------- */

    val libraryCount: Int get() = if (isClosed) 0 else AnimationNative.nativeLibraryCount(handle)

    fun libraryItem(index: Int): AnimationLibraryItem? {
        if (isClosed) return null
        val fields = AnimationNative.nativeLibraryEntry(handle, index)
        if (fields.size < 8) return null
        return AnimationLibraryItem(
            name = fields[0],
            category = AnimationCategory.fromNativeId(fields[1].toIntOrNull() ?: 0),
            durationSeconds = fields[2].toFloatOrNull() ?: 0f,
            looping = fields[3] == "1",
            authored = fields[4] == "1",
            source = fields[5],
            licence = fields[6],
            clipIndex = fields[7].toIntOrNull() ?: -1,
        )
    }

    fun library(): List<AnimationLibraryItem> = (0 until libraryCount).mapNotNull { libraryItem(it) }

    /** Searches the library by name, category label or source. [AnimationCategory.CATEGORY_ALL] for any. */
    fun searchLibrary(query: String, category: Int = AnimationCategory.CATEGORY_ALL): List<Int> =
        if (isClosed) emptyList()
        else AnimationNative.nativeLibrarySearch(handle, query, category).toList()

    /** How many library entries fall in each category, indexed by [AnimationCategory.nativeId]. */
    fun libraryHistogram(): IntArray =
        if (isClosed) IntArray(0) else AnimationNative.nativeLibraryHistogram(handle)

    /* ----------------------------------------------------------------- lifetime -------------- */

    override fun close() {
        if (closed) return
        closed = true
        AnimationNative.nativeDestroy(handle)
    }

    internal companion object {
        /* Layout of the per-frame float buffer, agreed with avatarforge_animation_jni.cpp. */
        const val FRAME_FLOATS = 10
    }
}

/** Entry point. Loads the native library once and reports honestly when it is unavailable. */
object AnimationEngine {

    val health: AnimationHealth by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
        val load = runCatching { System.loadLibrary("mega_hub_avatarforge") }
        val loadError = load.exceptionOrNull()
        if (loadError != null) {
            AnimationHealth(
                nativeLibraryLoaded = false,
                problem = "Native animation library could not load: " +
                    (loadError.message ?: loadError.javaClass.simpleName),
            )
        } else {
            /* A library that loads is not proof the animation symbols are in it. Creating and
               destroying a session exercises the actual entry points, so a build that linked without
               the animation JNI is reported here instead of crashing on first use in the UI. */
            runCatching {
                val probe = AnimationNative.nativeCreate()
                if (probe == 0L) error("nativeCreate returned no session")
                AnimationNative.nativeDestroy(probe)
                AnimationHealth(nativeLibraryLoaded = true, problem = null)
            }.getOrElse { failure ->
                AnimationHealth(
                    nativeLibraryLoaded = true,
                    problem = "Animation native probe failed: " +
                        (failure.message ?: failure.javaClass.simpleName),
                )
            }
        }
    }

    /**
     * Allocates the reusable per-frame buffer [AnimationSession.advance] writes into.
     *
     * A factory rather than a public constructor because [AnimationFrame] is filled by native code on the
     * render path and callers are meant to hold exactly one instance for the life of a view. Allocating
     * one per frame would put a Java object on a 60 Hz path for no reason.
     */
    fun newFrame(): AnimationFrame = AnimationFrame()

    /**
     * Creates a session, or null when the native library is unavailable. Callers must [AnimationSession.close]
     * what they create; a `use { }` block is the safe pattern for a one-shot import.
     */
    fun create(): AnimationSession? {
        if (!health.usable) return null
        val handle = runCatching { AnimationNative.nativeCreate() }.getOrDefault(0L)
        return if (handle == 0L) null else AnimationSession(handle)
    }

    fun categoryLabel(category: AnimationCategory): String =
        if (!health.usable) category.name
        else AnimationNative.nativeCategoryName(category.nativeId) ?: category.name

    fun categoryCount(): Int =
        if (!health.usable) 0 else AnimationNative.nativeCategoryCount()

    fun categoryForClipName(name: String): AnimationCategory =
        if (!health.usable) AnimationCategory.UNCATEGORIZED
        else AnimationCategory.fromNativeId(AnimationNative.nativeCategoryForClipName(name))

    fun expressionNames(): List<String> {
        if (!health.usable) return FacialExpression.entries.map { it.name }
        return (0 until AnimationNative.nativeExpressionCount())
            .mapNotNull { AnimationNative.nativeExpressionName(it) }
    }

    /** The 15-viseme mouth set names, for the lip-sync diagnostics panel. */
    fun visemeNames(): List<String> =
        if (!health.usable) emptyList()
        else (0 until 15).mapNotNull { AnimationNative.nativeVisemeName(it) }

    fun visemeForCharacter(text: String, index: Int): Int =
        if (!health.usable) 0 else AnimationNative.nativeVisemeForChar(text, index)
}

/**
 * Raw JNI declarations. Private to this file: the types above are the API, and every method here has
 * a matching `Java_com_davemega_hub_avatarforge_AnimationNative_<name>` symbol in
 * `avatarforge_animation_jni.cpp`. A mismatch is an [UnsatisfiedLinkError] at first call, not at link
 * time, so the two lists are kept adjacent and in the same order.
 */
internal object AnimationNative {
    external fun nativeCreate(): Long
    external fun nativeDestroy(handle: Long)
    external fun nativeLastError(handle: Long): String
    external fun nativeIsAlive(handle: Long): Boolean

    external fun nativeLoadGlb(handle: Long, bytes: ByteArray, source: String, licence: String): Int
    external fun nativeLoadGlbFile(handle: Long, path: String, source: String, licence: String): Int
    external fun nativeLoadAuthoredLibrary(handle: Long): Int
    external fun nativeLoadNotes(handle: Long): Array<String>

    external fun nativeClipCount(handle: Long): Int
    external fun nativeClipName(handle: Long, index: Int): String?
    external fun nativeClipDuration(handle: Long, index: Int): Float
    external fun nativeClipCategory(handle: Long, index: Int): Int
    external fun nativeClipLooping(handle: Long, index: Int): Boolean
    external fun nativeClipChannelCount(handle: Long, index: Int): Int
    external fun nativeClipHasRootMotion(handle: Long, index: Int): Boolean
    external fun nativeClipIndex(handle: Long, name: String): Int
    external fun nativeJointCount(handle: Long): Int
    external fun nativeSkinSlotCount(handle: Long): Int
    external fun nativeRolesMatched(handle: Long): Int
    external fun nativeJointName(handle: Long, index: Int): String?
    external fun nativeMorphCount(handle: Long): Int
    external fun nativeMorphName(handle: Long, index: Int): String?

    external fun nativePlayIndex(handle: Long, index: Int): Boolean
    external fun nativePlay(handle: Long, name: String): Boolean
    external fun nativePause(handle: Long)
    external fun nativeResume(handle: Long)
    external fun nativeStop(handle: Long)
    external fun nativeSeek(handle: Long, seconds: Float)
    external fun nativeSetSpeed(handle: Long, speed: Float): Boolean
    external fun nativeSpeed(handle: Long): Float
    external fun nativeSetLoop(handle: Long, loop: Boolean)
    external fun nativePosition(handle: Long): Float
    external fun nativeCurrentDuration(handle: Long): Float
    external fun nativeIsPlaying(handle: Long): Boolean
    external fun nativeCurrentClip(handle: Long): Int

    external fun nativeCrossfadeToIndex(handle: Long, index: Int, seconds: Float): Boolean
    external fun nativeCrossfadeTo(handle: Long, name: String, seconds: Float): Boolean
    external fun nativeSetCrossfadeEnabled(handle: Long, enabled: Boolean)
    external fun nativeBuildStateMachine(handle: Long, crossfadeSeconds: Float, allToAll: Boolean): Int
    external fun nativeAddTransition(handle: Long, from: String, to: String, seconds: Float): Boolean
    external fun nativeSetStateMachineDriven(handle: Long, driven: Boolean)
    external fun nativeRequestState(handle: Long, name: String): Boolean
    external fun nativeCurrentStateName(handle: Long): String?

    external fun nativeAddLayer(handle: Long, clipIndex: Int, weight: Float, additive: Boolean): Int
    external fun nativeLayerCount(handle: Long): Int
    external fun nativeSetLayerClip(handle: Long, layer: Int, clipIndex: Int): Boolean
    external fun nativeSetLayerWeight(handle: Long, layer: Int, weight: Float): Boolean
    external fun nativeSetLayerEnabled(handle: Long, layer: Int, enabled: Boolean): Boolean
    external fun nativeSetLayerAdditive(handle: Long, layer: Int, additive: Boolean): Boolean
    external fun nativeSetLayerMask(handle: Long, layer: Int, which: Int): Int
    external fun nativeAddEvent(handle: Long, clipIndex: Int, time: Float, name: String, value: Float): Boolean

    external fun nativeSetExpression(handle: Long, expressionId: Int, amount: Float): Int
    external fun nativeSetMorphWeight(handle: Long, name: String, value: Float): Boolean
    external fun nativeSetMorphWeightAt(handle: Long, index: Int, value: Float): Boolean
    external fun nativeMorphWeightAt(handle: Long, index: Int): Float
    external fun nativeResetFace(handle: Long)
    external fun nativeSetBlinkEnabled(handle: Long, enabled: Boolean)
    external fun nativeSetBlinkInterval(handle: Long, minSeconds: Float, maxSeconds: Float)
    external fun nativeTriggerBlink(handle: Long)
    external fun nativeSpeak(handle: Long, text: String, totalSeconds: Float): Int
    external fun nativeStopSpeaking(handle: Long)
    external fun nativeIsSpeaking(handle: Long): Boolean
    external fun nativeUnboundFaceChannels(handle: Long): Array<String>

    external fun nativeCheckCompatibility(handle: Long, otherHandle: Long): FloatArray
    external fun nativeRetarget(handle: Long, sourceHandle: Long, sourceClipIndex: Int, newName: String): Int

    external fun nativeAdvance(handle: Long, dt: Float, outFrame: FloatArray, outMorphs: FloatArray): Int
    external fun nativePreviousTime(handle: Long): Float
    external fun nativeCrossfadeAlpha(handle: Long): Float
    external fun nativeTransitionProgress(handle: Long): Float
    external fun nativeIsTransitioning(handle: Long): Boolean
    external fun nativeConsumeRootMotion(handle: Long): FloatArray
    external fun nativeSetRootMotionEnabled(handle: Long, enabled: Boolean)
    external fun nativeStats(handle: Long): DoubleArray
    external fun nativeResetStats(handle: Long)
    external fun nativeDrainEvents(handle: Long): Array<String>

    external fun nativeLibraryCount(handle: Long): Int
    external fun nativeLibraryEntry(handle: Long, index: Int): Array<String>
    external fun nativeLibrarySearch(handle: Long, query: String, category: Int): IntArray
    external fun nativeLibraryHistogram(handle: Long): IntArray

    external fun nativeCategoryName(category: Int): String?
    external fun nativeCategoryCount(): Int
    external fun nativeCategoryForClipName(name: String): Int
    external fun nativeExpressionName(id: Int): String?
    external fun nativeExpressionCount(): Int
    external fun nativeVisemeName(id: Int): String?
    external fun nativeVisemeForChar(text: String, index: Int): Int
    external fun nativeBonePalette(handle: Long, outMatrices: FloatArray): Int
}
