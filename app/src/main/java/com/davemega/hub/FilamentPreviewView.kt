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
    private var animator: Animator? = null
    private var animationCount = 0
    private var animationStartNanos = 0L

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
                    val activeAnimator = animator
                    if (activeAnimator != null && animationCount > 0) {
                        val duration = activeAnimator.getAnimationDuration(0)
                        if (duration.isFinite() && duration > 0f) {
                            val elapsedSeconds = ((frameTimeNanos - animationStartNanos).coerceAtLeast(0L)) / 1_000_000_000f
                            activeAnimator.applyAnimation(0, elapsedSeconds % duration)
                            activeAnimator.updateBoneMatrices()
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
        statusListener?.invoke(value)
    }

    private fun persistCheckpoint(value: String) {
        context.getSharedPreferences(DIAGNOSTICS_PREFS, Context.MODE_PRIVATE)
            .edit().putString(LAST_CHECKPOINT, value).commit()
        reportStatus(value)
        Log.i(TAG, "Renderer checkpoint: $value")
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        if (engine != null) return
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
            val width = surfaceView.width.coerceAtLeast(1)
            val height = surfaceView.height.coerceAtLeast(1)
            newCamera.setProjection(45.0, width.toDouble() / height.toDouble(), 0.05, 1000.0, Camera.Fov.VERTICAL)
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
                .direction(0.0f, -1.0f, -0.5f)
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
                val stream = when {
                    uri == null -> context.assets.open(BUNDLED_MODEL)
                    uri.scheme == "file" -> File(uri.path ?: error("Selected file URI has no path")).inputStream()
                    else -> context.contentResolver.openInputStream(uri)
                        ?: error("Android could not open the selected GLB URI")
                }
                bytes = readBounded(stream)
                if (bytes!!.isEmpty()) error("Selected GLB is empty")
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

    private fun loadModelBytes(bytes: ByteArray, uri: Uri?) {
        val loader = checkNotNull(assetLoader) { "glTF asset loader is unavailable" }
        val resources = checkNotNull(resourceLoader) { "glTF resource loader is unavailable" }
        val activeScene = checkNotNull(scene) { "Filament scene is unavailable" }

        destroyCurrentAsset()
        val newAsset = loader.createAsset(ByteBuffer.wrap(bytes))
            ?: error("Filament gltfio could not parse this file as glTF 2.0 / GLB")
        try {
            require(newAsset.resourceUris.isEmpty()) {
                "This viewer currently accepts self-contained GLB files only; the selected asset references external resources"
            }
            resources.loadResources(newAsset)
            newAsset.releaseSourceData()
            activeScene.addEntities(newAsset.entities)
            filamentAsset = newAsset
            animator = newAsset.instance.animator
            animationCount = animator?.animationCount ?: 0
            animationStartNanos = System.nanoTime()
            frameCamera(newAsset.boundingBox)
            val label = uri?.lastPathSegment ?: BUNDLED_MODEL
            reportStatus("GLB loaded and GPU resources submitted: $label · ${newAsset.entities.size} nodes · $animationCount animation(s)")
        } catch (failure: Exception) {
            activeScene.removeEntities(newAsset.entities)
            loader.destroyAsset(newAsset)
            throw failure
        } catch (failure: LinkageError) {
            activeScene.removeEntities(newAsset.entities)
            loader.destroyAsset(newAsset)
            throw failure
        }
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
        val current = filamentAsset ?: return
        scene?.removeEntities(current.entities)
        assetLoader?.let { loader -> safelyDestroy("glTF asset") { loader.destroyAsset(current) } }
        filamentAsset = null
        animator = null
        animationCount = 0
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        if (width <= 0 || height <= 0) return
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
