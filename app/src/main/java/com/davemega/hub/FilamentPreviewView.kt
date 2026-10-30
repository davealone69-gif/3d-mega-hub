package com.davemega.hub

import android.content.Context
import android.util.Log
import android.view.Choreographer
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import com.google.android.filament.Camera
import com.google.android.filament.Engine
import com.google.android.filament.Filament
import com.google.android.filament.Renderer
import com.google.android.filament.Scene
import com.google.android.filament.SwapChain
import com.google.android.filament.View
import com.google.android.filament.Viewport

/**
 * Minimal Filament clear-frame bring-up, preferring Vulkan and falling back to OpenGL ES.
 *
 * Initialise Filament's native runtime explicitly before touching Engine. The
 * Java wrappers can be present while the JNI library has not yet been loaded;
 * calling Engine.create() first then fails at Engine.nCreateBuilder().
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
                            persistCheckpoint("Filament $backendName first frame submitted; inspect viewport for pixels")
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

            persistCheckpoint(
                "Filament ${selection.backend.displayName()} initialized " +
                    "(Vulkan preferred, OpenGL ES fallback); starting clear-frame test"
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

            persistCheckpoint("Creating ${backend.displayName()} camera")
            candidateCameraEntity = com.google.android.filament.EntityManager.get().create()
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
            safelyDestroy("camera entity") { com.google.android.filament.EntityManager.get().destroy(activeCameraEntity) }
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
