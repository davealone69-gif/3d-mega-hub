package com.davemega.hub

import android.content.Context
import android.util.Log
import android.view.Choreographer
import android.view.SurfaceView
import android.widget.FrameLayout
import com.google.android.filament.Engine
import com.google.android.filament.Filament
import com.google.android.filament.utils.ModelViewer
import com.google.android.filament.utils.Utils
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** OpenGL-only Filament GLB viewport with persistent breadcrumbs for fatal native crashes. */
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

    private fun persistCheckpoint(value: String) {
        context.getSharedPreferences(DIAGNOSTICS_PREFS, Context.MODE_PRIVATE)
            .edit().putString(LAST_CHECKPOINT, value).commit()
        reportStatus(value)
        Log.i(TAG, "Renderer checkpoint: $value")
    }

    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            val current = viewer ?: return
            try {
                if (!firstFrameReported) {
                    context.getSharedPreferences(DIAGNOSTICS_PREFS, Context.MODE_PRIVATE)
                        .edit().putString(LAST_CHECKPOINT, "Calling ModelViewer.render() for first frame").commit()
                }
                val submitted = current.render(frameTimeNanos)
                if (submitted && !firstFrameReported) {
                    firstFrameReported = true
                    persistCheckpoint("First frame submitted; visible pixels still need visual confirmation")
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
        addView(surfaceView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        startViewer()
    }

    override fun onDetachedFromWindow() {
        Choreographer.getInstance().removeFrameCallback(frameCallback)
        surfaceView.setOnTouchListener(null)
        viewer = null
        super.onDetachedFromWindow()
    }

    private fun startViewer() {
        if (viewer != null) return
        try {
            persistCheckpoint("Entering Filament.init()")
            Filament.init()
            persistCheckpoint("Entering Utils.init() / filament-utils JNI load")
            Utils.init()
            persistCheckpoint("Entering Engine.create(OPENGL)")
            val engine = Engine.create(Engine.Backend.OPENGL)
            checkNotNull(engine) { "Engine.create(OPENGL) returned null" }
            persistCheckpoint("Entering ModelViewer constructor / Manipulator JNI")
            val next = ModelViewer(surfaceView, engine)
            persistCheckpoint("Reading models/female.glb from assets")
            val bytes = context.assets.open("models/female.glb").use { it.readBytes() }
            val buffer = ByteBuffer.allocateDirect(bytes.size).order(ByteOrder.nativeOrder())
            buffer.put(bytes)
            buffer.flip()
            persistCheckpoint("Entering ModelViewer.loadModelGlb()")
            next.loadModelGlb(buffer)
            check(next.asset != null) { "Filament could not create an asset from models/female.glb" }
            persistCheckpoint("Entering transformToUnitCube()")
            next.transformToUnitCube()
            surfaceView.setOnTouchListener(next)
            viewer = next
            firstFrameReported = false
            persistCheckpoint("GLB loaded; scheduling first render frame")
            Choreographer.getInstance().postFrameCallback(frameCallback)
        } catch (error: Throwable) {
            Log.e(TAG, "Could not start the Filament OpenGL viewport", error)
            val message = "Renderer unavailable: ${error.message ?: error.javaClass.simpleName}"
            context.getSharedPreferences(DIAGNOSTICS_PREFS, Context.MODE_PRIVATE)
                .edit().putString(LAST_CHECKPOINT, message).commit()
            reportStatus(message)
        }
    }

    companion object {
        private const val TAG = "MegaHubRenderer"
        const val DIAGNOSTICS_PREFS = "renderer_diagnostics"
        const val LAST_CHECKPOINT = "last_checkpoint"
    }
}
