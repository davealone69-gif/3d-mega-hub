package com.davemega.hub

import android.content.Context
import android.net.Uri
import android.util.Log
import android.view.Choreographer
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import com.google.android.filament.Camera
import com.google.android.filament.ColorGrading
import com.google.android.filament.Engine
import com.google.android.filament.EntityManager
import com.google.android.filament.Filament
import com.google.android.filament.IndirectLight
import com.google.android.filament.LightManager
import com.google.android.filament.Renderer
import com.google.android.filament.Scene
import com.google.android.filament.SwapChain
import com.google.android.filament.ToneMapper
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
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.sin

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
    private var indirectLight: IndirectLight? = null
    private var colorGrading: ColorGrading? = null
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

    private var running = false
    private var statusListener: ((String) -> Unit)? = null
    private var lastStatus = "Waiting for an Android surface."
    private var lastFrameTime = 0L

    private var modelUri: Uri? = null
    private var hasModelRequest = false
    private var modelLoadToken = 0

    // The orbit camera: where the loaded model was framed, and how the user has turned it since.
    private var framedCenterX = 0.0
    private var framedCenterY = 0.9
    private var framedCenterZ = 0.0
    private var framedRadius = 0.9f
    private var orbitYawDegrees = 0f
    private var orbitPitchDegrees = DEFAULT_PITCH_DEGREES
    private var orbitZoom = 1f
    private var lastTouchX = 0f
    private var lastTouchY = 0f
    private var activePointerId = MotionEvent.INVALID_POINTER_ID

    private val scaleDetector = ScaleGestureDetector(
        context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                val factor = detector.scaleFactor
                if (factor.isFinite() && factor > 0f) {
                    orbitZoom = (orbitZoom / factor).coerceIn(MIN_ZOOM, MAX_ZOOM)
                    applyOrbitCamera()
                }
                return true
            }
        }
    )

    private val tapDetector = GestureDetector(
        context,
        object : GestureDetector.SimpleOnGestureListener() {
            override fun onDoubleTap(e: MotionEvent): Boolean {
                orbitYawDegrees = 0f
                orbitPitchDegrees = DEFAULT_PITCH_DEGREES
                orbitZoom = 1f
                applyOrbitCamera()
                return true
            }
        }
    )

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
                    val activeAnimator = animator
                    if (activeAnimator != null && animationCount > 0) {
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

    private fun publishAnimationInfo() {
        val activeAnimator = animator
        val duration = if (activeAnimator != null && animationCount > 0) {
            activeAnimator.getAnimationDuration(selectedAnimation.coerceIn(0, animationCount - 1)).takeIf { it.isFinite() } ?: 0f
        } else 0f
        animationInfoListener?.invoke(
            animationCount, selectedAnimation, animationPlaying, animationLoop,
            animationSpeed, duration, animationPositionSeconds
        )
    }

    fun selectAnimation(index: Int) {
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
        animationPlaying = playing && animationCount > 0
        lastAnimationTickNanos = 0L
        publishAnimationInfo()
    }

    fun stopAnimation() {
        animationPlaying = false
        animationPositionSeconds = 0f
        lastAnimationTickNanos = 0L
        if (animationCount > 0) {
            animator?.applyAnimation(selectedAnimation.coerceIn(0, animationCount - 1), 0f)
            animator?.updateBoneMatrices()
        }
        publishAnimationInfo()
    }

    fun setAnimationLoop(loop: Boolean) {
        animationLoop = loop
        publishAnimationInfo()
    }

    fun setAnimationSpeed(speed: Float) {
        if (speed.isFinite() && speed > 0f) animationSpeed = speed.coerceIn(0.1f, 3.0f)
        publishAnimationInfo()
    }

    fun seekAnimation(positionSeconds: Float) {
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
    /** One finger turns the model, two fingers zoom, a double tap puts the camera back. */
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            parent?.requestDisallowInterceptTouchEvent(true)
        }
        tapDetector.onTouchEvent(event)
        scaleDetector.onTouchEvent(event)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                activePointerId = event.getPointerId(0)
                lastTouchX = event.x
                lastTouchY = event.y
            }
            MotionEvent.ACTION_MOVE -> {
                if (!scaleDetector.isInProgress && event.pointerCount == 1) {
                    val index = event.findPointerIndex(activePointerId)
                    if (index >= 0) {
                        val x = event.getX(index)
                        val y = event.getY(index)
                        val span = max(1, minOf(width, height)).toFloat()
                        orbitYawDegrees -= ((x - lastTouchX) / span) * 180.0f
                        orbitPitchDegrees = (orbitPitchDegrees + ((y - lastTouchY) / span) * 120.0f).coerceIn(-MAX_PITCH_DEGREES, MAX_PITCH_DEGREES)
                        lastTouchX = x
                        lastTouchY = y
                        applyOrbitCamera()
                    }
                }
            }
            MotionEvent.ACTION_POINTER_UP -> {
                val pointerIndex = event.actionIndex
                if (event.getPointerId(pointerIndex) == activePointerId) {
                    val newIndex = if (pointerIndex == 0) 1 else 0
                    if (newIndex < event.pointerCount) {
                        activePointerId = event.getPointerId(newIndex)
                        lastTouchX = event.getX(newIndex)
                        lastTouchY = event.getY(newIndex)
                    }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                activePointerId = MotionEvent.INVALID_POINTER_ID
                parent?.requestDisallowInterceptTouchEvent(false)
            }
        }
        return true
    }

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
            applyToneCurveAndBackground()
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
            configureViewQuality(newView)

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

    /**
     * Picture quality of the glTF viewer. Until 2026-11-15 Filament's post-processing was switched
     * off here, which left the picture without a tone curve, without anti-aliasing and without
     * ambient occlusion: linear light clipped at white, stair-stepped edges, flat creases. This
     * switches on Filament's own pipeline: 4x MSAA with FXAA on top, screen-space ambient
     * occlusion and shadows from the key light. The tone curve is chosen in
     * [applyToneCurveAndBackground].
     *
     * Every one of these is a switch on an existing Filament feature; if a backend rejects one,
     * the viewer falls back to the plain picture instead of failing to start.
     */
    private fun configureViewQuality(target: FilamentView) {
        try {
            target.setPostProcessingEnabled(true)
            target.setAntiAliasing(FilamentView.AntiAliasing.FXAA)
            val multisampling = FilamentView.MultiSampleAntiAliasingOptions()
            multisampling.enabled = true
            multisampling.sampleCount = 4
            target.setMultiSampleAntiAliasingOptions(multisampling)
            val occlusion = FilamentView.AmbientOcclusionOptions()
            occlusion.enabled = true
            target.setAmbientOcclusionOptions(occlusion)
            target.setShadowingEnabled(true)
        } catch (failure: Exception) {
            Log.w(TAG, "Viewer quality options were rejected; using the plain picture", failure)
            target.setPostProcessingEnabled(false)
        } catch (failure: LinkageError) {
            Log.w(TAG, "Viewer quality options are unavailable in this Filament build; using the plain picture", failure)
            target.setPostProcessingEnabled(false)
        }
    }

    /**
     * Tone curve and background of the viewer.
     *
     * Filament's default curve (ACES) pulls bright colours towards white. On the CI emulator the
     * brightest part of the bundled figure, whose base colour shows as (237, 213, 196), came out
     * as (214, 201, 188) with that curve and as (210, 185, 168) with the Khronos PBR Neutral
     * curve: red to blue is 1.21 in the file, 1.14 and 1.25 in the two pictures. The neutral curve
     * keeps hue and saturation and only rolls off the highlights, which is what a viewer for skin
     * and for models with painted textures needs.
     * (The message of commit 96152cd gives the file's colour as (216, 169, 140). That is the
     * linear factor times 255, not what a display shows, and it overstates the difference.)
     *
     * Without a skybox the background is the renderer's clear colour, and with post-processing on
     * it passes through the same curve. [STAGE_BEFORE_NEUTRAL_CURVE] is the creator's stage colour
     * (0xFF1C2230) before that curve, [STAGE_LINEAR] the same colour when nothing is applied to
     * it, so both previews stand on the same background. Until 2026-11-15 this one was black.
     */
    private fun applyToneCurveAndBackground() {
        val activeEngine = engine ?: return
        val activeView = view ?: return
        val activeRenderer = renderer ?: return
        var background = STAGE_LINEAR
        if (activeView.isPostProcessingEnabled) {
            try {
                val grading = ColorGrading.Builder()
                    .toneMapper(ToneMapper.PBRNeutralToneMapper())
                    .build(activeEngine)
                activeView.setColorGrading(grading)
                colorGrading = grading
                background = STAGE_BEFORE_NEUTRAL_CURVE
            } catch (failure: Exception) {
                Log.w(TAG, "The neutral tone curve could not be created; using Filament's default curve", failure)
            } catch (failure: LinkageError) {
                Log.w(TAG, "The neutral tone curve is unavailable in this Filament build; using its default curve", failure)
            }
        }
        try {
            val clearOptions = Renderer.ClearOptions()
            clearOptions.clearColor = background
            clearOptions.clear = true
            activeRenderer.setClearOptions(clearOptions)
        } catch (failure: Exception) {
            Log.w(TAG, "The viewer background colour could not be set", failure)
        } catch (failure: LinkageError) {
            Log.w(TAG, "The viewer background colour is unavailable in this Filament build", failure)
        }
    }

    private fun createSceneLight() {
        val activeEngine = engine ?: return
        val activeScene = scene ?: return
        val entity = EntityManager.get().create()
        try {
            // The key light casts shadows (nose, chin, arms across the body). One 2048 map over a
            // model about two metres high is roughly one texel per millimetre.
            val shadowOptions = LightManager.ShadowOptions()
            shadowOptions.mapSize = 2048
            LightManager.Builder(LightManager.Type.DIRECTIONAL)
                .color(1.0f, 0.96f, 0.90f)
                .intensity(75_000.0f)
                .direction(0.0f, -0.8944272f, -0.4472136f)
                .castShadows(true)
                .shadowOptions(shadowOptions)
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
        // One directional light leaves every surface that faces away from it black. This adds light
        // from all round as spherical harmonics: a uniform term and a little more from above. The
        // viewer works without it, so a failure here is logged and not fatal.
        try {
            val ambient = IndirectLight.Builder()
                .irradiance(
                    2,
                    floatArrayOf(
                        0.80f, 0.80f, 0.84f,
                        0.16f, 0.17f, 0.20f,
                        0.00f, 0.00f, 0.00f,
                        0.00f, 0.00f, 0.00f
                    )
                )
                .intensity(AMBIENT_INTENSITY)
                .build(activeEngine)
            activeScene.indirectLight = ambient
            indirectLight = ambient
        } catch (failure: Exception) {
            Log.w(TAG, "Ambient light could not be created; continuing with the key light only", failure)
        } catch (failure: LinkageError) {
            Log.w(TAG, "Ambient light native call failed; continuing with the key light only", failure)
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
                val file = if (uri != null && uri.scheme == "file") {
                    File(uri.path ?: error("Selected file URI has no path"))
                } else {
                    null
                }
                if (file != null) {
                    // Decided from the JSON chunk and the image headers, before the model is read:
                    //  - gltfio reserves morph storage for every vertex of every target, and an avatar
                    //    this app generated needs several hundred megabytes of it. Asking for that
                    //    ended the process on the phone;
                    //  - it decodes every texture to RGBA with a mip chain, and a model from a desktop
                    //    tool can need more than a gigabyte.
                    // Such a file is turned away here, whoever sent it.
                    require(file.length() <= MAX_MODEL_BYTES) {
                        "GLB exceeds the viewer's ${MAX_MODEL_BYTES / (1024L * 1024L)} MiB safety limit"
                    }
                    GlbPreviewRouting.animationPreviewRefusal(GlbPreviewRouting.summarize(file))
                        ?.let { reason -> error("this preview cannot open the file: $reason") }
                    // readBytes allocates the file's length once; the stream path below doubles it.
                    bytes = file.readBytes()
                } else {
                    val stream = if (uri == null) {
                        context.assets.open(BUNDLED_MODEL)
                    } else {
                        context.contentResolver.openInputStream(uri)
                            ?: error("Android could not open the selected GLB URI")
                    }
                    bytes = readBounded(stream)
                }
                if (bytes!!.isEmpty()) error("Selected GLB is empty")
                GlbValidator.validate(bytes!!)
                if (file == null) {
                    GlbPreviewRouting.animationPreviewRefusal(
                        GlbPreviewRouting.summarize(java.io.ByteArrayInputStream(bytes!!))
                    )?.let { reason -> error("this preview cannot open the file: $reason") }
                }
            } catch (error: Exception) {
                failure = error
            } catch (error: OutOfMemoryError) {
                bytes = null
                failure = IllegalStateException("the file is too large to read into memory")
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

    private fun readBounded(input: InputStream): ByteArray {
        input.use { source ->
            val output = ByteArrayOutputStream()
            val buffer = ByteArray(64 * 1024)
            var total = 0L
            while (true) {
                val count = source.read(buffer)
                if (count < 0) break
                total += count
                require(total <= MAX_MODEL_BYTES) {
                    "GLB exceeds the viewer's ${MAX_MODEL_BYTES / (1024L * 1024L)} MiB safety limit"
                }
                output.write(buffer, 0, count)
            }
            return output.toByteArray()
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
            reportStatus("GLB loaded and GPU resources submitted: $label · ${pending.entities.size} nodes · $animationCount animation(s) · drag to turn, pinch to zoom, double-tap to reset")
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
        val center = bounds.center
        val half = bounds.halfExtent
        framedRadius = max(half[0], max(half[1], half[2])).takeIf { it.isFinite() && it > 0.01f } ?: 0.9f
        framedCenterX = center[0].toDouble()
        framedCenterY = center[1].toDouble()
        framedCenterZ = center[2].toDouble()
        orbitYawDegrees = 0f
        orbitPitchDegrees = DEFAULT_PITCH_DEGREES
        orbitZoom = 1f
        applyOrbitCamera()
    }

    /**
     * Puts the camera on a sphere round the framed model: yaw about the vertical axis, pitch above
     * or below the horizon, zoom as a factor on the framing distance. The near and far planes follow
     * the distance, so a model authored in centimetres is framed as well as one in metres.
     */
    private fun applyOrbitCamera() {
        val activeCamera = camera ?: return
        val distance = ((framedRadius * 3.2f).coerceIn(0.2f, 5000.0f) * orbitZoom).toDouble()
        val width = surfaceView.width.coerceAtLeast(1)
        val height = surfaceView.height.coerceAtLeast(1)
        activeCamera.setProjection(
            45.0,
            width.toDouble() / height.toDouble(),
            max(0.01, distance * 0.01),
            max(1000.0, distance * 20.0),
            Camera.Fov.VERTICAL
        )
        val yaw = Math.toRadians(orbitYawDegrees.toDouble())
        val pitch = Math.toRadians(orbitPitchDegrees.toDouble())
        activeCamera.lookAt(
            framedCenterX + distance * cos(pitch) * sin(yaw),
            framedCenterY + distance * sin(pitch),
            framedCenterZ + distance * cos(pitch) * cos(yaw),
            framedCenterX, framedCenterY, framedCenterZ,
            0.0, 1.0, 0.0
        )
    }

    private fun destroyCurrentAsset() {
        destroyPendingAsset()
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
        applyOrbitCamera()
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
        indirectLight?.let { ambient ->
            safelyDestroy("ambient light removal") { scene?.indirectLight = null }
            safelyDestroy("ambient light") { activeEngine.destroyIndirectLight(ambient) }
        }
        indirectLight = null
        colorGrading?.let { grading ->
            safelyDestroy("tone curve removal") { view?.setColorGrading(null) }
            safelyDestroy("tone curve") { activeEngine.destroyColorGrading(grading) }
        }
        colorGrading = null
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
        private const val AMBIENT_INTENSITY = 15_000.0f
        /** The creator's stage colour 0xFF1C2230 in linear light, alpha last. */
        private val STAGE_LINEAR = doubleArrayOf(0.0116, 0.0160, 0.0296, 1.0)
        /**
         * The colour that the PBR Neutral curve, applied in Filament's Rec. 2020 grading space,
         * turns into the stage colour. The curve subtracts a toe from dark colours, so the input
         * is lighter than the result.
         */
        private val STAGE_BEFORE_NEUTRAL_CURVE = doubleArrayOf(0.0448, 0.0492, 0.0628, 1.0)
        private const val DEFAULT_PITCH_DEGREES = 2.15f
        private const val MAX_PITCH_DEGREES = 85.0f
        private const val MIN_ZOOM = 0.15f
        private const val MAX_ZOOM = 4.0f
        const val DIAGNOSTICS_PREFS = "renderer_diagnostics"
        const val LAST_CHECKPOINT = "last_checkpoint"
    }
}
