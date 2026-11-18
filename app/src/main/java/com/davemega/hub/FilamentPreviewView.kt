package com.davemega.hub

import android.content.Context
import android.net.Uri
import android.util.Log
import android.view.Choreographer
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import com.google.android.filament.Camera
import com.google.android.filament.Engine
import com.google.android.filament.EntityManager
import com.google.android.filament.Filament
import com.google.android.filament.LightManager
import com.google.android.filament.Renderer
import com.google.android.filament.Scene
import com.google.android.filament.SwapChain
import com.google.android.filament.Viewport
import com.google.android.filament.View as FilamentView
import com.google.android.filament.gltfio.Animator
import com.google.android.filament.gltfio.AssetLoader
import com.google.android.filament.gltfio.FilamentAsset
import com.google.android.filament.gltfio.Gltfio
import com.google.android.filament.gltfio.ResourceLoader
import com.google.android.filament.gltfio.UbershaderProvider
import com.davemega.hub.avatarforge.GlbValidator
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.InputStream
import java.nio.ByteBuffer
import kotlin.concurrent.thread
import kotlin.math.max

/** Filament gltfio viewer. GLES is primary; Vulkan is an optional fallback. */
class FilamentPreviewView(context: Context) : FrameLayout(context), SurfaceHolder.Callback {
    private val surfaceView = SurfaceView(context)

    private var engine: Engine? = null
    private var renderer: Renderer? = null
    private var scene: Scene? = null
    private var view: FilamentView? = null
    private var camera: Camera? = null
    private var swapChain: SwapChain? = null
    private var cameraEntity = 0
    private var lightEntity = 0
    private var activeBackend: Engine.Backend? = null

    private var materialProvider: UbershaderProvider? = null
    private var assetLoader: AssetLoader? = null
    private var resourceLoader: ResourceLoader? = null
    private var filamentAsset: FilamentAsset? = null
    private var pendingAsset: FilamentAsset? = null
    private var pendingAssetUri: Uri? = null
    private var lastReportedLoadProgress = -1
    private var animator: Animator? = null
    private var animationCount = 0
    private var selectedAnimation = 0
    private var animationPositionSeconds = 0f
    private var animationPlaying = true
    private var animationLoop = true
    private var animationSpeed = 1.0f
    private var lastAnimationTickNanos = 0L
    private var animationFrameCounter = 0
    private var animationInfoListener: ((Int, Int, Boolean, Boolean, Float, Float, Float) -> Unit)? = null

    /* Native animation core integration. Off by default so the existing gltfio-only playback path is
       unchanged for callers that do not opt in; see setNativeAnimationEnabled. */
    private var nativeAnimationEnabled = false
    private var nativeDriver: NativeAnimationDriver? = null
    /* Retained so the core can parse the same bytes gltfio did. Both parsers must see one file, or the
       clip lists being matched would not describe the same asset. */
    private var loadedModelBytes: ByteArray? = null

    private var running = false
    private var statusListener: ((String) -> Unit)? = null
    private var lastStatus = "Waiting for an Android surface."
    private var lastFrameTime = 0L

    private var modelUri: Uri? = null
    private var hasModelRequest = false
    private var modelLoadToken = 0

    private data class RendererSession(
        val engine: Engine,
        val renderer: Renderer,
        val scene: Scene,
        val view: FilamentView,
        val camera: Camera,
        val cameraEntity: Int,
        val swapChain: SwapChain
    )

    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            if (!running) return
            val activeRenderer = renderer
            val activeSwapChain = swapChain
            val activeView = view
            if (activeRenderer != null && activeSwapChain != null && activeView != null) {
                try {
                    updatePendingAssetLoad()
                    val driver = nativeDriver
                    if (driver != null && driver.isAttached) {
                        /* The core owns the clocks and the blend; gltfio still samples and skins. When the
                           core has nothing to apply this frame (idle, or a clip with no renderer
                           counterpart), fall through to the gltfio path rather than freezing the model. */
                        if (driver.tick(frameTimeNanos)) {
                            animationFrameCounter++
                            lastAnimationTickNanos = frameTimeNanos
                            if (animationFrameCounter % 15 == 0) publishAnimationInfo()
                        }
                    }
                    val activeAnimator = animator
                    val driverHandledFrame = driver != null && driver.isAttached
                    if (!driverHandledFrame && activeAnimator != null && animationCount > 0) {
                        val index = selectedAnimation.coerceIn(0, animationCount - 1)
                        val duration = activeAnimator.getAnimationDuration(index)
                        if (duration.isFinite() && duration > 0f) {
                            val elapsedSeconds = if (lastAnimationTickNanos == 0L) 0f else
                                ((frameTimeNanos - lastAnimationTickNanos).coerceAtLeast(0L)) / 1_000_000_000f
                            lastAnimationTickNanos = frameTimeNanos
                            if (animationPlaying) {
                                animationPositionSeconds += elapsedSeconds * animationSpeed
                                if (animationLoop) {
                                    animationPositionSeconds %= duration
                                } else if (animationPositionSeconds >= duration) {
                                    animationPositionSeconds = duration
                                    animationPlaying = false
                                }
                            }
                            activeAnimator.applyAnimation(index, animationPositionSeconds.coerceIn(0f, duration))
                            activeAnimator.updateBoneMatrices()
                            animationFrameCounter++
                            if (animationFrameCounter % 15 == 0) publishAnimationInfo()
                        }
                    }
                    if (activeRenderer.beginFrame(activeSwapChain, frameTimeNanos)) {
                        activeRenderer.render(activeView)
                        activeRenderer.endFrame()
                        if (lastFrameTime == 0L) {
                            lastFrameTime = frameTimeNanos
                            val backendName = activeBackend?.displayName() ?: "unknown backend"
                            persistCheckpoint("Filament $backendName frame submitted; model-pixel visibility still requires device confirmation")
                        }
                    }
                } catch (error: Exception) {
                    Log.e(TAG, "Filament frame failed", error)
                    reportStatus("Frame error: ${error.message ?: error.javaClass.simpleName}")
                    running = false
                    return
                } catch (error: LinkageError) {
                    Log.e(TAG, "Filament frame JNI failure", error)
                    persistCheckpoint("Renderer JNI failure during frame: ${error.message ?: error.javaClass.simpleName}")
                    running = false
                    return
                }
            }
            if (running) Choreographer.getInstance().postFrameCallback(this)
        }
    }

    init {
        addView(surfaceView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
        surfaceView.holder.addCallback(this)
    }

    fun setStatusListener(listener: (String) -> Unit) {
        statusListener = listener
        listener(lastStatus)
    }

    fun setAnimationInfoListener(listener: (Int, Int, Boolean, Boolean, Float, Float, Float) -> Unit) {
        animationInfoListener = listener
        publishAnimationInfo()
    }

    /**
     * Reports playback state to the UI. When the native core owns the clock, everything reported comes
     * from the core: its clip count, its current clip, its position and its duration. Reporting gltfio's
     * numbers while the core drives playback would put a progress bar on the screen that measures a clock
     * nothing is reading.
     */
    private fun publishAnimationInfo() {
        val driver = nativeDriver?.takeIf { it.isAttached }
        val session = driver?.session()
        if (driver != null && session != null) {
            animationInfoListener?.invoke(
                driver.clipCount, session.currentClip, session.isPlaying, animationLoop,
                session.speed, session.currentDuration, session.position,
            )
            return
        }
        val activeAnimator = animator
        val duration = if (activeAnimator != null && animationCount > 0) {
            activeAnimator.getAnimationDuration(selectedAnimation.coerceIn(0, animationCount - 1)).takeIf { it.isFinite() } ?: 0f
        } else 0f
        animationInfoListener?.invoke(
            animationCount, selectedAnimation, animationPlaying, animationLoop,
            animationSpeed, duration, animationPositionSeconds
        )
    }

    /**
     * Hands the published asset to the native animation core.
     *
     * Failure here is reported and the view keeps working through gltfio's own Animator -- a character
     * that animates with only its own clips is more useful than one that shows nothing because a second
     * parser disagreed. [nativeAnimationStatus] carries the reason either way.
     */
    private fun attachNativeDriverIfEnabled(published: FilamentAsset, label: String) {
        nativeDriver?.detach()
        if (!nativeAnimationEnabled) {
            nativeDriver = null
            return
        }
        val bytes = loadedModelBytes
        val activeAnimator = animator
        val activeEngine = engine
        if (bytes == null || activeAnimator == null || activeEngine == null) {
            nativeAnimationStatus = "the native animation core was not attached: no asset bytes, animator or engine"
            return
        }
        val driver = NativeAnimationDriver(activeEngine.renderableManager)
        val attached = runCatching {
            driver.attach(published, activeAnimator, bytes, label)
        }.getOrElse { failure ->
            nativeAnimationStatus = "native animation attach failed: ${failure.message ?: failure.javaClass.simpleName}"
            false
        }
        nativeDriver = if (attached) driver else null
        nativeAnimationStatus = driver.status
        if (attached) {
            val limitations = driver.limitations
            if (limitations.isNotEmpty()) {
                reportStatus("native animation: ${limitations.joinToString(" | ")}")
            }
        } else {
            reportStatus("native animation unavailable: $nativeAnimationStatus")
        }
    }

    /**
     * Opts this view into driving animation from the native `af::anim` core instead of gltfio's Animator
     * alone. Takes effect on the next asset load, or immediately when an asset is already published.
     *
     * What opting in adds: crossfades with verified alpha semantics, a state machine with declared
     * edges, animation layers, autonomous blinking, expression presets, approximate lip-sync, root-motion
     * readback, retargeting onto another character, per-frame timing and clip categories. What it does
     * not change: gltfio still samples and skins the skeleton, so the two parsers are never asked to
     * agree about joint order.
     */
    fun setNativeAnimationEnabled(enabled: Boolean) {
        if (nativeAnimationEnabled == enabled) return
        nativeAnimationEnabled = enabled
        val published = filamentAsset
        if (!enabled) {
            nativeDriver?.detach()
            nativeDriver = null
            nativeAnimationStatus = "native animation core disabled; gltfio Animator drives playback"
            return
        }
        if (published != null) {
            val label = modelUri?.lastPathSegment ?: BUNDLED_MODEL
            attachNativeDriverIfEnabled(published, label)
        } else {
            nativeAnimationStatus = "native animation core enabled; attaches when an asset is loaded"
        }
    }

    fun isNativeAnimationEnabled(): Boolean = nativeAnimationEnabled

    /** The driver, when attached. Null means gltfio's Animator is the only thing driving playback. */
    fun nativeAnimationDriver(): NativeAnimationDriver? = nativeDriver

    /** Why the native core is or is not driving this view. Always meaningful, never empty. */
    var nativeAnimationStatus: String = "native animation core disabled; gltfio Animator drives playback"
        private set

    /**
     * Selects the clip to play.
     *
     * The index space depends on who owns the clock, and [publishAnimationInfo] reports the matching
     * count, so a picker built from that count always passes indices this method understands:
     *  - with the native core attached, these are CORE clip indices (the richer list, with categories);
     *  - otherwise they are gltfio animation indices, as before.
     */
    fun selectAnimation(index: Int) {
        val driver = nativeDriver?.takeIf { it.isAttached }
        if (driver != null) {
            val coreCount = driver.clipCount
            if (coreCount <= 0) return
            selectedAnimation = index.coerceIn(0, coreCount - 1)
            animationPositionSeconds = 0f
            if (!driver.play(selectedAnimation)) {
                reportStatus("the animation core refused clip $selectedAnimation: ${driver.status}")
                return
            }
            animationPlaying = true
            lastAnimationTickNanos = 0L
            publishAnimationInfo()
            return
        }
        if (animationCount <= 0) return
        selectedAnimation = index.coerceIn(0, animationCount - 1)
        animationPositionSeconds = 0f
        animationPlaying = true
        lastAnimationTickNanos = 0L
        animator?.let {
            it.applyAnimation(selectedAnimation, 0f)
            it.updateBoneMatrices()
        }
        publishAnimationInfo()
    }

    fun setAnimationPlaying(playing: Boolean) {
        val driver = nativeDriver?.takeIf { it.isAttached }
        if (driver != null) {
            if (playing) driver.resume() else driver.pause()
            animationPlaying = playing
            lastAnimationTickNanos = 0L
            publishAnimationInfo()
            return
        }
        animationPlaying = playing && animationCount > 0
        lastAnimationTickNanos = 0L
        publishAnimationInfo()
    }

    fun stopAnimation() {
        animationPlaying = false
        animationPositionSeconds = 0f
        lastAnimationTickNanos = 0L
        nativeDriver?.takeIf { it.isAttached }?.let { driver ->
            driver.stop()
            publishAnimationInfo()
            return
        }
        if (animationCount > 0) {
            animator?.applyAnimation(selectedAnimation.coerceIn(0, animationCount - 1), 0f)
            animator?.updateBoneMatrices()
        }
        publishAnimationInfo()
    }

    fun setAnimationLoop(loop: Boolean) {
        animationLoop = loop
        nativeDriver?.takeIf { it.isAttached }?.setLoop(loop)
        publishAnimationInfo()
    }

    /**
     * Playback speed. The core clamps to [0.05, 8] and REFUSES a negative or non-finite value rather
     * than applying it, so a refusal is reported instead of leaving the UI showing a speed the character
     * is not running at.
     */
    fun setAnimationSpeed(speed: Float) {
        if (speed.isFinite() && speed > 0f) animationSpeed = speed.coerceIn(0.1f, 3.0f)
        val driver = nativeDriver?.takeIf { it.isAttached }
        if (driver != null && !driver.setSpeed(speed)) {
            reportStatus("the animation core refused speed $speed: ${driver.status}")
        }
        publishAnimationInfo()
    }

    fun seekAnimation(positionSeconds: Float) {
        val driver = nativeDriver?.takeIf { it.isAttached }
        if (driver != null) {
            driver.seek(positionSeconds)
            val session = driver.session()
            animationPositionSeconds = session?.position ?: positionSeconds
            /* Seeking does not resume a paused clip: dragging the timeline must not start playback the
               user did not ask for. The core enforces this, so the flag is read back rather than set. */
            animationPlaying = session?.isPlaying ?: animationPlaying
            lastAnimationTickNanos = 0L
            publishAnimationInfo()
            return
        }
        val activeAnimator = animator ?: return
        if (animationCount <= 0) return
        val duration = activeAnimator.getAnimationDuration(selectedAnimation.coerceIn(0, animationCount - 1))
        if (!duration.isFinite() || duration <= 0f) return
        animationPositionSeconds = positionSeconds.coerceIn(0f, duration)
        activeAnimator.applyAnimation(selectedAnimation, animationPositionSeconds)
        activeAnimator.updateBoneMatrices()
        lastAnimationTickNanos = 0L
        publishAnimationInfo()
    }

    /** A null URI selects the bundled sample GLB. */
    fun setModel(uri: Uri?) {
        if (hasModelRequest && modelUri == uri) return
        hasModelRequest = true
        modelUri = uri
        val token = ++modelLoadToken
        if (engine == null) {
            reportStatus(if (uri == null) "Bundled sample GLB queued until renderer startup" else "Selected GLB queued until renderer startup")
        } else {
            loadModelAsync(uri, token)
        }
    }

    private fun reportStatus(value: String) {
        lastStatus = value
        Log.i(TAG, "Renderer status: $value")
        statusListener?.invoke(value)
    }

    private fun persistCheckpoint(value: String) {
        // Checkpoints are diagnostic breadcrumbs, never a reason to block the UI/render thread.
        context.getSharedPreferences(DIAGNOSTICS_PREFS, Context.MODE_PRIVATE)
            .edit().putString(LAST_CHECKPOINT, value).apply()
        reportStatus(value)
        Log.i(TAG, "Renderer checkpoint: $value")
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        if (engine != null) return
        // SurfaceCreated can arrive before layout has assigned pixel dimensions. Do not create
        // a Filament swap chain or projection using fabricated 1x1 dimensions.
        if (surfaceView.width <= 0 || surfaceView.height <= 0) {
            reportStatus("Waiting for non-zero viewport dimensions before creating the Filament swap chain.")
            return
        }
        initializeFilament(holder)
    }

    private fun initializeFilament(holder: SurfaceHolder) {
        if (engine != null || surfaceView.width <= 0 || surfaceView.height <= 0) return
        try {
            persistCheckpoint("Loading Filament native runtime")
            Filament.init()
            Gltfio.init()
            persistCheckpoint("Filament and gltfio JNI loaded; initializing OpenGL ES first")
            val selection = createWithRendererBackendFallback { backend ->
                try {
                    createRendererSession(holder.surface, backend)
                } catch (failure: Exception) {
                    val nextStep = if (backend == Engine.Backend.OPENGL) "trying optional Vulkan fallback" else "no backend remains"
                    Log.w(TAG, "Filament ${backend.displayName()} initialization failed; $nextStep", failure)
                    persistCheckpoint("Filament ${backend.displayName()} initialization failed; $nextStep")
                    throw failure
                } catch (failure: LinkageError) {
                    val nextStep = if (backend == Engine.Backend.OPENGL) "trying optional Vulkan fallback" else "no backend remains"
                    Log.w(TAG, "Filament ${backend.displayName()} native initialization failed; $nextStep", failure)
                    persistCheckpoint("Filament ${backend.displayName()} native initialization failed; $nextStep")
                    throw failure
                }
            }
            val session = selection.value
            engine = session.engine
            renderer = session.renderer
            scene = session.scene
            view = session.view
            camera = session.camera
            cameraEntity = session.cameraEntity
            swapChain = session.swapChain
            activeBackend = selection.backend

            createSceneLight()
            initializeGltfLoaders()
            persistCheckpoint("Filament ${selection.backend.displayName()} session initialized; starting GLB viewer")
            if (!hasModelRequest) {
                hasModelRequest = true
                modelUri = null
                modelLoadToken++
            }
            loadModelAsync(modelUri, modelLoadToken)
            running = true
            lastFrameTime = 0L
            Choreographer.getInstance().postFrameCallback(frameCallback)
        } catch (error: Exception) {
            Log.e(TAG, "Filament renderer initialization failed", error)
            persistCheckpoint("Renderer unavailable: ${error.message ?: error.javaClass.simpleName}")
            releaseEngine()
        } catch (error: LinkageError) {
            Log.e(TAG, "Filament native library/linkage failure", error)
            persistCheckpoint("Renderer native library failure: ${error.message ?: error.javaClass.simpleName}")
            releaseEngine()
        }
    }

    private fun createRendererSession(surface: Surface, backend: Engine.Backend): RendererSession {
        val candidateEngine = checkNotNull(Engine.create(backend)) {
            "Engine.create(${backend.displayName()}) returned null"
        }
        var candidateRenderer: Renderer? = null
        var candidateScene: Scene? = null
        var candidateView: FilamentView? = null
        var candidateCamera: Camera? = null
        var candidateSwapChain: SwapChain? = null
        var candidateCameraEntity = 0

        try {
            val newRenderer = checkNotNull(candidateEngine.createRenderer()) { "Filament renderer was null" }
            candidateRenderer = newRenderer
            val newScene = checkNotNull(candidateEngine.createScene()) { "Filament scene was null" }
            candidateScene = newScene
            val newView = checkNotNull(candidateEngine.createView()) { "Filament view was null" }
            candidateView = newView
            newView.setPostProcessingEnabled(false)

            candidateCameraEntity = EntityManager.get().create()
            val newCamera = checkNotNull(candidateEngine.createCamera(candidateCameraEntity)) {
                "Filament camera was null"
            }
            candidateCamera = newCamera
            // Dimensions are validated before session creation; use the real buffer size.
            val width = surfaceView.width
            val height = surfaceView.height
            check(width > 0 && height > 0) { "Filament viewport has zero dimensions ($width x $height)" }
            newCamera.setProjection(45.0, width.toDouble() / height.toDouble(), 0.05, 1000.0, Camera.Fov.VERTICAL)
            // Filament camera exposure uses photographic parameters, not a signed EV scalar.
            // This neutral baseline avoids an accidental zero/negative exposure configuration.
            newCamera.setExposure(16.0f, 1.0f / 125.0f, 100.0f)
            newCamera.lookAt(0.0, 1.0, 3.5, 0.0, 0.9, 0.0, 0.0, 1.0, 0.0)
            newView.camera = newCamera
            newView.scene = newScene

            val newSwapChain = checkNotNull(candidateEngine.createSwapChain(surface)) {
                "Filament ${backend.displayName()} swap chain was null"
            }
            candidateSwapChain = newSwapChain
            newView.setViewport(Viewport(0, 0, width, height))
            return RendererSession(candidateEngine, newRenderer, newScene, newView, newCamera, candidateCameraEntity, newSwapChain)
        } catch (failure: Exception) {
            destroyResources(candidateEngine, candidateSwapChain, candidateView, candidateScene, candidateCamera, candidateCameraEntity, candidateRenderer)
            throw failure
        } catch (failure: LinkageError) {
            destroyResources(candidateEngine, candidateSwapChain, candidateView, candidateScene, candidateCamera, candidateCameraEntity, candidateRenderer)
            throw failure
        }
    }

    private fun createSceneLight() {
        val activeEngine = engine ?: return
        val activeScene = scene ?: return
        val entity = EntityManager.get().create()
        try {
            LightManager.Builder(LightManager.Type.DIRECTIONAL)
                .color(1.0f, 0.96f, 0.90f)
                .intensity(75_000.0f)
                .direction(0.0f, -0.8944272f, -0.4472136f)
                .castShadows(false)
                .build(activeEngine, entity)
            activeScene.addEntity(entity)
            lightEntity = entity
        } catch (failure: Exception) {
            EntityManager.get().destroy(entity)
            throw failure
        } catch (failure: LinkageError) {
            EntityManager.get().destroy(entity)
            throw failure
        }
    }

    private fun initializeGltfLoaders() {
        val activeEngine = checkNotNull(engine)
        val provider = UbershaderProvider(activeEngine)
        materialProvider = provider
        assetLoader = AssetLoader(activeEngine, provider, EntityManager.get())
        resourceLoader = ResourceLoader(activeEngine)
    }

    private fun loadModelAsync(uri: Uri?, token: Int) {
        val activeEngine = engine ?: return
        reportStatus(if (uri == null) "Reading bundled sample GLB" else "Reading selected GLB")
        thread(name = "MegaHubGlbReader") {
            var bytes: ByteArray? = null
            var failure: Exception? = null
            try {
                val (stream, declaredLength) = openModel(uri)
                bytes = readBounded(stream, declaredLength)
                if (bytes!!.isEmpty()) error("Selected GLB is empty")
                GlbValidator.validate(bytes!!)
            } catch (error: Exception) {
                failure = error
            }
            post {
                if (token != modelLoadToken || engine !== activeEngine) return@post
                if (failure != null) {
                    reportStatus("GLB read failed: ${failure!!.message ?: failure!!.javaClass.simpleName}")
                } else {
                    try {
                        loadModelBytes(checkNotNull(bytes), uri)
                    } catch (error: Exception) {
                        Log.e(TAG, "GLB load failed", error)
                        reportStatus("GLB load failed: ${error.message ?: error.javaClass.simpleName}")
                    } catch (error: LinkageError) {
                        Log.e(TAG, "GLB native load failed", error)
                        reportStatus("GLB native load failed: ${error.message ?: error.javaClass.simpleName}")
                    }
                }
            }
        }
    }

    /**
     * Opens the model to display and reports its length when Android knows it, as -1 when it does not.
     *
     * The length is not an optimisation. Without it the reader has to grow a buffer by doubling, and
     * that is what produced `OutOfMemoryError: Failed to allocate a 134217744 byte allocation ...
     * growth limit 201326592` in the generated-avatar diagnostic.
     */
    private fun openModel(uri: Uri?): Pair<InputStream, Long> {
        if (uri == null) {
            /* openFd throws for a compressed asset, which is a legitimate "length unknown", not a
               failure to open - the open() below is the one that has to succeed. */
            val length: Long = runCatching {
                context.assets.openFd(BUNDLED_MODEL).use { it.length }
            }.getOrDefault(-1L)
            return context.assets.open(BUNDLED_MODEL) to length
        }
        if (uri.scheme == "file") {
            val file = File(uri.path ?: error("Selected file URI has no path"))
            return file.inputStream() to file.length()
        }
        val length: Long = runCatching {
            context.contentResolver.openFileDescriptor(uri, "r")?.use { it.statSize } ?: -1L
        }.getOrDefault(-1L)
        val stream = context.contentResolver.openInputStream(uri)
            ?: error("Android could not open the selected GLB URI")
        return stream to length
    }

    /**
     * Reads a GLB into ONE byte array.
     *
     * The version this replaced streamed into a ByteArrayOutputStream and failed on the emulator with a
     * 128 MiB allocation against a 192 MiB heap growth limit. Two separate things were wrong:
     *
     *  * A doubling buffer holds the old array and the new one at the same time, so crossing 64 MiB
     *    needs 64 + 128 MiB live, and toByteArray() then copies the result a third time. Peak usage was
     *    about three times the file size.
     *  * MAX_MODEL_BYTES was 128 MiB on a 192 MiB heap, so the "safety limit" described a file this
     *    viewer could never actually read. A bound that cannot be honoured is not a bound. The check
     *    now measures the heap that is really available and names it in the failure.
     *
     * With a known length this allocates exactly once. Without one it still streams, pre-sized, and
     * enforces the same bound on every chunk.
     */
    private fun readBounded(input: InputStream, declaredLength: Long): ByteArray {
        input.use { source ->
            val heapLimit = Runtime.getRuntime().maxMemory()
            if (declaredLength > 0L) {
                requireModelBytesFit(declaredLength, heapLimit)
                /* Safe to narrow: requireModelBytesFit has already rejected anything above
                   MAX_MODEL_BYTES, which is far under Int.MAX_VALUE. */
                val exact = ByteArray(declaredLength.toInt())
                var offset = 0
                while (offset < exact.size) {
                    val count = source.read(exact, offset, exact.size - offset)
                    if (count < 0) break
                    offset += count
                }
                /* A stat size that overshoots the real file is a short read, not a corrupt model. */
                return if (offset == exact.size) exact else exact.copyOf(offset)
            }
            val output = ByteArrayOutputStream(64 * 1024)
            val buffer = ByteArray(64 * 1024)
            var total = 0L
            while (true) {
                val count = source.read(buffer)
                if (count < 0) break
                total += count
                requireModelBytesFit(total, heapLimit)
                output.write(buffer, 0, count)
            }
            return output.toByteArray()
        }
    }

    /**
     * Refuses a model that cannot fit, with the numbers in the message.
     *
     * The heap is shared with Filament, the decoded textures (the imported avatar's two WebP images
     * decode to 32 MiB) and the native core's own copy of the file, so the file alone is budgeted half
     * of it. Failing here reports the size, the budget and the heap; failing inside the allocator
     * reported only "OutOfMemoryError" with no way to act on it.
     */
    private fun requireModelBytesFit(bytes: Long, heapLimit: Long) {
        val mib = 1024L * 1024L
        require(bytes <= MAX_MODEL_BYTES) {
            "GLB is ${bytes / mib} MiB, above the viewer's ${MAX_MODEL_BYTES / mib} MiB limit"
        }
        val budget = heapLimit / 2L
        require(bytes <= budget) {
            "GLB is ${bytes / mib} MiB but only ${budget / mib} MiB of the ${heapLimit / mib} MiB " +
                "Dalvik heap is budgeted for it; the rest is needed for Filament, decoded textures " +
                "and the native core"
        }
    }

    /**
     * Parses the GLB on the UI/Filament owner thread, but never synchronously decodes and uploads
     * all textures here. ResourceLoader.loadResources() is synchronous and blocks until texture
     * decoding completes. The emulator trace recorded a 45-second UI frame while this synchronous
     * path was in use, so asyncBeginLoad + asyncUpdateLoad now spreads resource work across frames.
     */
    private fun loadModelBytes(bytes: ByteArray, uri: Uri?) {
        val loader = checkNotNull(assetLoader) { "glTF asset loader is unavailable" }
        val resources = checkNotNull(resourceLoader) { "glTF resource loader is unavailable" }

        destroyPendingAsset()
        destroyCurrentAsset()
        /* Retained after the destroys above, which clear the previous asset's bytes. The core parses
           the very same array gltfio just parsed, so the two clip lists being matched describe one
           file rather than two that happen to share a name. */
        loadedModelBytes = bytes
        val newAsset = loader.createAsset(ByteBuffer.wrap(bytes))
            ?: error("Filament gltfio could not parse this file as glTF 2.0 / GLB")
        try {
            require(newAsset.resourceUris.isEmpty()) {
                "This viewer currently accepts self-contained GLB files only; the selected asset references external resources"
            }
            check(resources.asyncBeginLoad(newAsset)) {
                "Filament could not start asynchronous GLB resource loading"
            }
            pendingAsset = newAsset
            pendingAssetUri = uri
            lastReportedLoadProgress = -1
            reportStatus("GLB parsed; loading textures and GPU resources asynchronously")
        } catch (failure: Exception) {
            loader.destroyAsset(newAsset)
            throw failure
        } catch (failure: LinkageError) {
            loader.destroyAsset(newAsset)
            throw failure
        }
    }

    private fun updatePendingAssetLoad() {
        val pending = pendingAsset ?: return
        val resources = resourceLoader ?: return
        try {
            resources.asyncUpdateLoad()
            val progress = resources.asyncGetLoadProgress().coerceIn(0.0f, 1.0f)
            val bucket = (progress * 10.0f).toInt()
            if (bucket > lastReportedLoadProgress && bucket < 10) {
                lastReportedLoadProgress = bucket
                reportStatus("Loading GLB resources · ${bucket * 10}%")
            }
            if (progress < 1.0f) return

            // asyncUpdateLoad performs the final main-thread upload work. Only publish the asset
            // to the scene after the loader reports 100% completion.
            resources.asyncUpdateLoad()
            pending.releaseSourceData()
            val activeScene = checkNotNull(scene) { "Filament scene is unavailable" }
            activeScene.addEntities(pending.entities)
            animator = pending.instance.animator
            animationCount = animator?.animationCount ?: 0
            selectedAnimation = 0
            animationPositionSeconds = 0f
            animationPlaying = animationCount > 0
            animationLoop = true
            lastAnimationTickNanos = 0L
            animationFrameCounter = 0
            animator?.takeIf { animationCount > 0 }?.let {
                it.applyAnimation(0, 0f)
                it.updateBoneMatrices()
            }
            publishAnimationInfo()
            frameCamera(pending.boundingBox)
            filamentAsset = pending
            pendingAsset = null
            val label = pendingAssetUri?.lastPathSegment ?: BUNDLED_MODEL
            pendingAssetUri = null
            lastReportedLoadProgress = -1
            attachNativeDriverIfEnabled(pending, label)
            reportStatus("GLB loaded and GPU resources submitted: $label · ${pending.entities.size} nodes · $animationCount animation(s)")
        } catch (failure: Exception) {
            Log.e(TAG, "Asynchronous GLB resource load failed", failure)
            destroyPendingAsset()
            reportStatus("GLB resource load failed: ${failure.message ?: failure.javaClass.simpleName}")
        } catch (failure: LinkageError) {
            Log.e(TAG, "Asynchronous GLB native resource load failed", failure)
            destroyPendingAsset()
            reportStatus("GLB native resource load failed: ${failure.message ?: failure.javaClass.simpleName}")
        }
    }

    private fun destroyPendingAsset() {
        val pending = pendingAsset ?: return
        resourceLoader?.let { resources ->
            safelyDestroy("pending glTF resource load") { resources.asyncCancelLoad() }
        }
        assetLoader?.let { loader ->
            safelyDestroy("pending glTF asset") { loader.destroyAsset(pending) }
        }
        pendingAsset = null
        pendingAssetUri = null
        lastReportedLoadProgress = -1
    }

    private fun frameCamera(bounds: com.google.android.filament.Box) {
        val activeCamera = camera ?: return
        val center = bounds.center
        val half = bounds.halfExtent
        val radius = max(half[0], max(half[1], half[2])).takeIf { it.isFinite() && it > 0.01f } ?: 0.9f
        val distance = (radius * 3.2f).coerceIn(1.0f, 80.0f)
        val width = surfaceView.width.coerceAtLeast(1)
        val height = surfaceView.height.coerceAtLeast(1)
        activeCamera.setProjection(45.0, width.toDouble() / height.toDouble(), 0.03, max(1000.0, distance.toDouble() * 20.0), Camera.Fov.VERTICAL)
        activeCamera.lookAt(
            center[0].toDouble(), (center[1] + radius * 0.12f).toDouble(), (center[2] + distance).toDouble(),
            center[0].toDouble(), center[1].toDouble(), center[2].toDouble(),
            0.0, 1.0, 0.0
        )
    }

    private fun destroyCurrentAsset() {
        destroyPendingAsset()
        nativeDriver?.detach()
        nativeDriver = null
        loadedModelBytes = null
        val current = filamentAsset ?: return
        scene?.removeEntities(current.entities)
        assetLoader?.let { loader -> safelyDestroy("glTF asset") { loader.destroyAsset(current) } }
        filamentAsset = null
        animator = null
        animationCount = 0
        selectedAnimation = 0
        animationPositionSeconds = 0f
        animationPlaying = false
        lastAnimationTickNanos = 0L
        publishAnimationInfo()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        if (width <= 0 || height <= 0) {
            reportStatus("Filament viewport dimensions are invalid: $width x $height")
            return
        }
        // surfaceChanged is the first reliable point at which SurfaceView has its real size.
        if (engine == null && holder.surface.isValid) {
            initializeFilament(holder)
            return
        }
        view?.setViewport(Viewport(0, 0, width, height))
        camera?.setProjection(45.0, width.toDouble() / height.toDouble(), 0.03, 1000.0, Camera.Fov.VERTICAL)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        releaseEngine()
    }

    private fun stopRendering() {
        running = false
        Choreographer.getInstance().removeFrameCallback(frameCallback)
    }

    private fun releaseEngine() {
        modelLoadToken++
        stopRendering()
        val activeEngine = engine ?: return
        destroyCurrentAsset()
        resourceLoader?.let { safelyDestroy("glTF resource loader") { it.destroy() } }
        resourceLoader = null
        assetLoader?.let { safelyDestroy("glTF asset loader") { it.destroy() } }
        assetLoader = null
        materialProvider?.let { provider ->
            safelyDestroy("glTF cached materials") { provider.destroyMaterials() }
            safelyDestroy("glTF material provider") { provider.destroy() }
        }
        materialProvider = null

        if (lightEntity != 0) {
            safelyDestroy("scene light removal") { scene?.removeEntity(lightEntity) }
            safelyDestroy("scene light component") { activeEngine.getLightManager().destroy(lightEntity) }
            safelyDestroy("scene light entity") { EntityManager.get().destroy(lightEntity) }
        }
        lightEntity = 0
        destroyResources(activeEngine, swapChain, view, scene, camera, cameraEntity, renderer)
        engine = null
        renderer = null
        scene = null
        view = null
        camera = null
        swapChain = null
        cameraEntity = 0
        activeBackend = null
    }

    private fun destroyResources(
        activeEngine: Engine,
        activeSwapChain: SwapChain?,
        activeView: FilamentView?,
        activeScene: Scene?,
        activeCamera: Camera?,
        activeCameraEntity: Int,
        activeRenderer: Renderer?
    ) {
        activeSwapChain?.let { safelyDestroy("swap chain") { activeEngine.destroySwapChain(it) } }
        activeView?.let { safelyDestroy("view") { activeEngine.destroyView(it) } }
        activeScene?.let { safelyDestroy("scene") { activeEngine.destroyScene(it) } }
        if (activeCamera != null && activeCameraEntity != 0) {
            safelyDestroy("camera component") { activeEngine.destroyCameraComponent(activeCameraEntity) }
        }
        if (activeCameraEntity != 0) {
            safelyDestroy("camera entity") { EntityManager.get().destroy(activeCameraEntity) }
        }
        activeRenderer?.let { safelyDestroy("renderer") { activeEngine.destroyRenderer(it) } }
        safelyDestroy("engine") { activeEngine.destroy() }
    }

    private fun safelyDestroy(resource: String, destroy: () -> Unit) {
        try {
            destroy()
        } catch (error: Exception) {
            Log.w(TAG, "Filament $resource cleanup failed", error)
        } catch (error: LinkageError) {
            Log.w(TAG, "Filament $resource native cleanup failed", error)
        }
    }

    override fun onDetachedFromWindow() {
        surfaceView.holder.removeCallback(this)
        releaseEngine()
        super.onDetachedFromWindow()
    }

    companion object {
        private const val TAG = "MegaHubRenderer"
        private const val BUNDLED_MODEL = "models/female.glb"
        private const val MAX_MODEL_BYTES = 128L * 1024L * 1024L
        const val DIAGNOSTICS_PREFS = "renderer_diagnostics"
        const val LAST_CHECKPOINT = "last_checkpoint"
    }
}
