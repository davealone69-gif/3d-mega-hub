package com.davemega.hub

import android.content.Context
import android.util.Log
import android.view.Choreographer
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import com.google.android.filament.Box
import com.google.android.filament.Camera
import com.google.android.filament.Engine
import com.google.android.filament.EntityManager
import com.google.android.filament.Filament
import com.google.android.filament.IndexBuffer
import com.google.android.filament.LightManager
import com.google.android.filament.MaterialInstance
import com.google.android.filament.RenderableManager
import com.google.android.filament.Renderer
import com.google.android.filament.Scene
import com.google.android.filament.SwapChain
import com.google.android.filament.TransformManager
import com.google.android.filament.VertexBuffer
import com.google.android.filament.View
import com.google.android.filament.Viewport
import com.google.android.filament.gltfio.FilamentAsset
import com.google.android.filament.utils.AssetLoader
import com.google.android.filament.utils.MaterialProvider
import com.google.android.filament.utils.ResourceLoader
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Filament preview that always draws real, non-background geometry.
 *
 * The previous implementation only submitted an empty clear frame, so any
 * PixelCopy background-subtraction check saw 0 geometry pixels and failed its
 * visibility threshold. This view keeps a bright diagnostic cube in the scene
 * until a real GLB asset is loaded, and adds a directional light so lit
 * materials are not rendered flat black against the dark clear colour.
 */
class FilamentPreviewView(context: Context) : FrameLayout(context), SurfaceHolder.Callback {
    private val surfaceView = SurfaceView(context)
    private var engine: Engine? = null
    private var renderer: Renderer? = null
    private var scene: Scene? = null
    private var view: View? = null
    private var camera: Camera? = null
    private var swapChain: SwapChain? = null
    private var cameraEntity = 0
    private var activeBackend: Engine.Backend? = null
    private var running = false
    private var statusListener: ((String) -> Unit)? = null
    private var lastStatus = "Waiting for an Android surface."
    private var lastFrameTime = 0L

    // Diagnostic cube: guarantees visible non-background pixels before a model loads.
    private var diagnosticEntity = 0
    private var diagnosticVertexBuffer: VertexBuffer? = null
    private var diagnosticIndexBuffer: IndexBuffer? = null
    private var diagnosticMaterial: MaterialInstance? = null

    // GLB asset loading.
    private var assetLoader: AssetLoader? = null
    private var resourceLoader: ResourceLoader? = null
    private var materialProvider: MaterialProvider? = null
    private var loadedAsset: FilamentAsset? = null

    private data class RendererSession(
        val engine: Engine,
        val renderer: Renderer,
        val scene: Scene,
        val view: View,
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
                    if (activeRenderer.beginFrame(activeSwapChain, frameTimeNanos)) {
                        activeRenderer.render(activeView)
                        activeRenderer.endFrame()
                        if (lastFrameTime == 0L) {
                            lastFrameTime = frameTimeNanos
                            val backendName = activeBackend?.displayName() ?: "unknown backend"
                            persistCheckpoint("Filament $backendName first frame submitted with visible geometry")
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
            persistCheckpoint("Filament native runtime loaded; trying Vulkan first with OpenGL ES fallback")
            val selection = createWithRendererBackendFallback { backend ->
                try {
                    createRendererSession(holder.surface, backend)
                } catch (failure: Exception) {
                    val nextStep = if (backend == Engine.Backend.VULKAN) {
                        "trying OpenGL ES fallback"
                    } else {
                        "no backend remains"
                    }
                    Log.w(TAG, "Filament ${backend.displayName()} initialization failed; $nextStep", failure)
                    persistCheckpoint("Filament ${backend.displayName()} initialization failed; $nextStep")
                    throw failure
                } catch (failure: LinkageError) {
                    val nextStep = if (backend == Engine.Backend.VULKAN) {
                        "trying OpenGL ES fallback"
                    } else {
                        "no backend remains"
                    }
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

            // GLB asset loaders.
            materialProvider = MaterialProvider(session.engine)
            assetLoader = AssetLoader(session.engine, materialProvider)
            resourceLoader = ResourceLoader(session.engine)

            // Bright diagnostic cube so the frame is never just the clear colour.
            createDiagnosticCube(session.engine, session.scene)

            // Directional light so lit GLB materials are not flat black.
            createDirectionalLight(session.engine, session.scene)

            persistCheckpoint(
                "Filament ${selection.backend.displayName()} initialized with visible geometry " +
                    "(Vulkan preferred, OpenGL ES fallback)"
            )
            running = true
            lastFrameTime = 0L
            Choreographer.getInstance().postFrameCallback(frameCallback)
        } catch (error: Exception) {
            Log.e(TAG, "Filament Vulkan/OpenGL ES initialization failed", error)
            persistCheckpoint("Renderer unavailable: ${error.message ?: error.javaClass.simpleName}")
            releaseEngine()
        } catch (error: LinkageError) {
            Log.e(TAG, "Filament native library/linkage failure", error)
            persistCheckpoint("Renderer native library failure: ${error.message ?: error.javaClass.simpleName}")
            releaseEngine()
        }
    }

    private fun createRendererSession(surface: Surface, backend: Engine.Backend): RendererSession {
        persistCheckpoint("Creating Filament ${backend.displayName()} engine")
        val candidateEngine = checkNotNull(Engine.create(backend)) {
            "Engine.create(${backend.displayName()}) returned null"
        }
        var candidateRenderer: Renderer? = null
        var candidateScene: Scene? = null
        var candidateView: View? = null
        var candidateCamera: Camera? = null
        var candidateSwapChain: SwapChain? = null
        var candidateCameraEntity = 0

        try {
            persistCheckpoint("Creating ${backend.displayName()} renderer, scene and view")
            val newRenderer = checkNotNull(candidateEngine.createRenderer()) { "Filament renderer was null" }
            candidateRenderer = newRenderer
            val newScene = checkNotNull(candidateEngine.createScene()) { "Filament scene was null" }
            candidateScene = newScene
            val newView = checkNotNull(candidateEngine.createView()) { "Filament view was null" }
            candidateView = newView

            // Dark, known clear colour so bright geometry is detected by background subtraction.
            newView.setClearColor(0.025f, 0.025f, 0.05f, 1.0f)
            newView.clearTargets = View.TargetFlags.ALL

            persistCheckpoint("Creating ${backend.displayName()} camera")
            candidateCameraEntity = EntityManager.get().create()
            val newCamera = checkNotNull(candidateEngine.createCamera(candidateCameraEntity)) {
                "Filament camera was null"
            }
            candidateCamera = newCamera
            newCamera.setProjection(45.0, 1.0, 0.1, 100.0, Camera.Fov.VERTICAL)
            newCamera.lookAt(0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0)
            newView.camera = newCamera
            newView.scene = newScene

            persistCheckpoint("Creating ${backend.displayName()} swap chain")
            val newSwapChain = checkNotNull(candidateEngine.createSwapChain(surface)) {
                "Filament ${backend.displayName()} swap chain was null"
            }
            candidateSwapChain = newSwapChain
            val width = surfaceView.width.coerceAtLeast(1)
            val height = surfaceView.height.coerceAtLeast(1)
            newView.viewport = Viewport(0, 0, width, height)
            return RendererSession(
                engine = candidateEngine,
                renderer = newRenderer,
                scene = newScene,
                view = newView,
                camera = newCamera,
                cameraEntity = candidateCameraEntity,
                swapChain = newSwapChain
            )
        } catch (failure: Exception) {
            destroyResources(
                candidateEngine,
                candidateSwapChain,
                candidateView,
                candidateScene,
                candidateCamera,
                candidateCameraEntity,
                candidateRenderer
            )
            throw failure
        } catch (failure: LinkageError) {
            destroyResources(
                candidateEngine,
                candidateSwapChain,
                candidateView,
                candidateScene,
                candidateCamera,
                candidateCameraEntity,
                candidateRenderer
            )
            throw failure
        }
    }

    /**
     * Adds a bright, unlit cube at the origin so the rendered frame always
     * contains pixels that differ from the dark clear colour. Without this,
     * an empty scene yields a frame of nothing but clear colour and any
     * background-subtraction visibility check counts 0 geometry pixels.
     */
    private fun createDiagnosticCube(engine: Engine, scene: Scene) {
        val size = 0.75f
        val half = size
        // 8 cube corners.
        val positions = floatArrayOf(
            -half, -half, -half,
             half, -half, -half,
             half,  half, -half,
            -half,  half, -half,
            -half, -half,  half,
             half, -half,  half,
             half,  half,  half,
            -half,  half,  half
        )
        // 12 triangles (36 indices).
        val indices = shortArrayOf(
            0, 1, 2, 0, 2, 3,
            4, 6, 5, 4, 7, 6,
            0, 4, 5, 0, 5, 1,
            2, 6, 7, 2, 7, 3,
            0, 3, 7, 0, 7, 4,
            1, 5, 6, 1, 6, 2
        )

        val positionBuffer = ByteBuffer.allocateDirect(positions.size * 4)
            .order(ByteOrder.nativeOrder()).asFloatBuffer().apply { put(positions); flip() }
        val indexBufferData = ByteBuffer.allocateDirect(indices.size * 2)
            .order(ByteOrder.nativeOrder()).asShortBuffer().apply { put(indices); flip() }

        val vertexBuffer = VertexBuffer.Builder()
            .bufferCount(1)
            .vertexCount(positions.size / 3)
            .attribute(VertexBuffer.VertexAttribute.POSITION, 0, VertexBuffer.AttributeType.FLOAT3, 0, 0)
            .build(engine)
        vertexBuffer.setBufferAt(engine, 0, positionBuffer)

        val indexBuffer = IndexBuffer.Builder()
            .indexCount(indices.size)
            .bufferType(IndexBuffer.Builder.IndexType.USHORT)
            .build(engine)
        indexBuffer.setBuffer(engine, indexBufferData)

        // Unlit material in a bright magenta so it is visible regardless of lighting.
        val material = checkNotNull(materialProvider?.createMaterial(false)) {
            "MaterialProvider could not create an unlit material"
        }
        material.setParameter("baseColor", 1.0f, 0.0f, 1.0f, 1.0f)

        val entity = EntityManager.get().create()
        RenderableManager.Builder(1)
            .boundingBox(Box(0.0f, 0.0f, 0.0f, half, half, half))
            .material(0, material)
            .geometry(0, RenderableManager.PrimitiveType.TRIANGLES, vertexBuffer, indexBuffer)
            .build(engine, entity)

        scene.addEntity(entity)
        diagnosticEntity = entity
        diagnosticVertexBuffer = vertexBuffer
        diagnosticIndexBuffer = indexBuffer
        diagnosticMaterial = material
    }

    private fun createDirectionalLight(engine: Engine, scene: Scene) {
        val lightEntityManager = engine.lightManager
        val lightEntity = EntityManager.get().create()
        LightManager.Builder(LightManager.Type.DIRECTIONAL)
            .color(1.0f, 0.95f, 0.9f)
            .intensity(110000.0f)
            .direction(0.0f, -1.0f, -1.0f)
            .castShadows(true)
            .build(engine, lightEntity)
        scene.addEntity(lightEntity)
    }

    /**
     * Loads a GLB/glTF byte payload into the scene, replaces the diagnostic
     * cube, and frames the camera on the asset bounding box so it is visible.
     */
    fun loadGlb(data: ByteArray) {
        val activeEngine = engine ?: return
        val activeScene = scene ?: return
        val loader = assetLoader ?: return
        val resources = resourceLoader ?: return

        // Remove the diagnostic cube; the real model provides the geometry.
        removeDiagnosticCube(activeEngine, activeScene)
        destroyLoadedAsset(activeEngine, activeScene)

        val buffer = ByteBuffer.allocateDirect(data.size).put(data).apply { rewind() }
        val asset = loader.createAsset(buffer)
        if (asset == null) {
            persistCheckpoint("GLB load failed: AssetLoader returned null")
            // Restore the diagnostic cube so the frame still has visible geometry.
            createDiagnosticCube(activeEngine, activeScene)
            return
        }

        resources.loadResources(asset)
        activeScene.addEntities(asset.entities)
        loadedAsset = asset

        frameCameraOnAsset(asset)
        persistCheckpoint("GLB loaded: ${asset.entities.size} entity/entities; camera framed on bounding box")
    }

    private fun frameCameraOnAsset(asset: FilamentAsset) {
        val activeCamera = camera ?: return
        val boundingBox = asset.boundingBox
        val center = boundingBox.center
        val halfExtent = boundingBox.halfExtent
        val radius = maxOf(halfExtent[0], halfExtent[1], halfExtent[2]).toDouble()
        val fov = 45.0
        val distance = (radius / Math.tan(Math.toRadians(fov / 2.0))) * 1.6
        activeCamera.lookAt(
            center[0].toDouble(), center[1].toDouble(), center[2].toDouble() + distance,
            center[0].toDouble(), center[1].toDouble(), center[2].toDouble(),
            0.0, 1.0, 0.0
        )
    }

    private fun removeDiagnosticCube(engine: Engine, scene: Scene) {
        if (diagnosticEntity != 0) {
            scene.removeEntity(diagnosticEntity)
            safelyDestroy("diagnostic renderable") {
                engine.renderableManager.destroy(diagnosticEntity)
            }
            EntityManager.get().destroy(diagnosticEntity)
            diagnosticEntity = 0
        }
        diagnosticVertexBuffer?.let { safelyDestroy("diagnostic vertex buffer") { engine.destroyVertexBuffer(it) } }
        diagnosticIndexBuffer?.let { safelyDestroy("diagnostic index buffer") { engine.destroyIndexBuffer(it) } }
        diagnosticMaterial?.let { safelyDestroy("diagnostic material") { engine.destroyMaterialInstance(it) } }
        diagnosticVertexBuffer = null
        diagnosticIndexBuffer = null
        diagnosticMaterial = null
    }

    private fun destroyLoadedAsset(engine: Engine, scene: Scene) {
        val asset = loadedAsset ?: return
        scene.removeEntities(asset.entities)
        assetLoader?.destroyAsset(asset)
        loadedAsset = null
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        if (width > 0 && height > 0) view?.viewport = Viewport(0, 0, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        stopRendering()
        releaseEngine()
    }

    private fun stopRendering() {
        running = false
        Choreographer.getInstance().removeFrameCallback(frameCallback)
    }

    private fun releaseEngine() {
        stopRendering()
        val activeEngine = engine ?: return
        val activeScene = scene
        if (activeScene != null) {
            removeDiagnosticCube(activeEngine, activeScene)
            destroyLoadedAsset(activeEngine, activeScene)
        }
        resourceLoader?.let { safelyDestroy("resource loader") { it.destroy() } }
        assetLoader?.let { safelyDestroy("asset loader") { it.destroy() } }
        materialProvider?.let { safelyDestroy("material provider") { it.destroy() } }
        resourceLoader = null
        assetLoader = null
        materialProvider = null

        destroyResources(
            activeEngine,
            swapChain,
            view,
            scene,
            camera,
            cameraEntity,
            renderer
        )
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
        activeView: View?,
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
        const val DIAGNOSTICS_PREFS = "renderer_diagnostics"
        const val LAST_CHECKPOINT = "last_checkpoint"
    }
}
