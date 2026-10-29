package com.davemega.hub

import android.content.Context
import android.util.Log
import android.view.Choreographer
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
 * Minimal Filament OpenGL ES bring-up.
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
    private var running = false
    private var statusListener: ((String) -> Unit)? = null
    private var lastStatus = "Waiting for an Android surface."
    private var lastFrameTime = 0L

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
                            persistCheckpoint("Filament OpenGL ES first frame submitted; inspect viewport for pixels")
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
            persistCheckpoint("Filament native runtime loaded; creating OpenGL ES engine")
            val newEngine = Engine.create(Engine.Backend.OPENGL)
            checkNotNull(newEngine) { "Engine.create(OPENGL) returned null" }
            engine = newEngine

            persistCheckpoint("Creating Filament renderer, scene and view")
            renderer = newEngine.createRenderer()
            scene = newEngine.createScene()
            view = newEngine.createView()

            persistCheckpoint("Creating camera entity")
            cameraEntity = com.google.android.filament.EntityManager.get().create()
            camera = newEngine.createCamera(cameraEntity)
            camera?.setProjection(45.0, 1.0, 0.1, 100.0, Camera.Fov.VERTICAL)
            camera?.lookAt(0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0)
            view?.camera = camera
            view?.scene = scene

            persistCheckpoint("Creating OpenGL ES swap chain")
            swapChain = newEngine.createSwapChain(holder.surface)
            val width = surfaceView.width.coerceAtLeast(1)
            val height = surfaceView.height.coerceAtLeast(1)
            view?.viewport = Viewport(0, 0, width, height)

            persistCheckpoint("Filament OpenGL ES initialized; starting clear-frame test")
            running = true
            lastFrameTime = 0L
            Choreographer.getInstance().postFrameCallback(frameCallback)
        } catch (error: Exception) {
            Log.e(TAG, "Filament OpenGL ES initialization failed", error)
            persistCheckpoint("Renderer unavailable: ${error.message ?: error.javaClass.simpleName}")
            releaseEngine()
        } catch (error: LinkageError) {
            Log.e(TAG, "Filament native library/linkage failure", error)
            persistCheckpoint("Renderer native library failure: ${error.message ?: error.javaClass.simpleName}")
            releaseEngine()
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
        try {
            swapChain?.let { activeEngine.destroySwapChain(it) }
            swapChain = null
            view?.let { activeEngine.destroyView(it) }
            view = null
            scene?.let { activeEngine.destroyScene(it) }
            scene = null
            if (cameraEntity != 0) {
                activeEngine.destroyCameraComponent(cameraEntity)
                com.google.android.filament.EntityManager.get().destroy(cameraEntity)
                cameraEntity = 0
            }
            renderer?.let { activeEngine.destroyRenderer(it) }
            renderer = null
            activeEngine.destroy()
        } catch (error: Exception) {
            Log.w(TAG, "Filament cleanup failed", error)
        } finally {
            engine = null
            camera = null
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
