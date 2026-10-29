package com.davemega.hub

import android.content.Context
import android.util.Log
import android.view.Choreographer
import android.view.SurfaceView
import android.widget.FrameLayout
import com.google.android.filament.Engine
import com.google.android.filament.Filament
import com.google.android.filament.utils.ModelViewer
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Minimal real GLB viewport. It deliberately requests Filament's OpenGL backend,
 * not automatic backend selection, because Vulkan has not been verified on the target phone.
 */
class FilamentPreviewView(context: Context) : FrameLayout(context) {
    private val surfaceView = SurfaceView(context)
    private var viewer: ModelViewer? = null
    private var firstFrameReported = false
    private var lastStatus = "Waiting for the OpenGL ES viewport."
    private var statusListener: ((String) -> Unit)? = null

    fun setStatusListener(listener: (String) -> Unit) {
        statusListener = listener
        listener(lastStatus)
    }

    private fun reportStatus(value: String) {
        lastStatus = value
        statusListener?.invoke(value)
    }

    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            val current = viewer ?: return
            try {
                val submitted = current.render(frameTimeNanos)
                if (submitted && !firstFrameReported) {
                    firstFrameReported = true
                    reportStatus("OpenGL ES frame submitted. Visible pixels still require device verification.")
                }
            } catch (error: Throwable) {
                Log.e(TAG, "Filament render loop failed", error)
                reportStatus("Renderer error: ${error.message ?: error.javaClass.simpleName}")
                return
            }
            if (viewer != null) Choreographer.getInstance().postFrameCallback(this)
        }
    }

    init {
        addView(
            surfaceView,
            LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT)
        )
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        startViewer()
    }

    override fun onDetachedFromWindow() {
        Choreographer.getInstance().removeFrameCallback(frameCallback)
        surfaceView.setOnTouchListener(null)
        // ModelViewer registers its own SurfaceView detach listener and destroys its engine
        // there. Clear our reference before child detachment so re-attachment creates a fresh
        // viewer instead of reusing a destroyed engine.
        viewer = null
        super.onDetachedFromWindow()
    }

    private fun startViewer() {
        if (viewer != null) return
        try {
            Filament.init()
            val engine = Engine.create(Engine.Backend.OPENGL)
            val next = ModelViewer(surfaceView, engine)
            val bytes = context.assets.open("models/female.glb").use { it.readBytes() }
            val buffer = ByteBuffer.allocateDirect(bytes.size).order(ByteOrder.nativeOrder())
            buffer.put(bytes)
            buffer.flip()
            next.loadModelGlb(buffer)
            check(next.asset != null) { "Filament could not create an asset from models/female.glb" }
            next.transformToUnitCube()
            surfaceView.setOnTouchListener(next)
            viewer = next
            firstFrameReported = false
            reportStatus("Filament OpenGL backend selected; CC0 GLB loaded. Waiting for first frame.")
            Choreographer.getInstance().postFrameCallback(frameCallback)
        } catch (error: Throwable) {
            Log.e(TAG, "Could not start the Filament OpenGL viewport", error)
            reportStatus("Renderer unavailable: ${error.message ?: error.javaClass.simpleName}")
        }
    }

    companion object {
        private const val TAG = "MegaHubRenderer"
    }
}
