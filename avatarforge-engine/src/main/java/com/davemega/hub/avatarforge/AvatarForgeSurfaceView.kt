package com.davemega.hub.avatarforge

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.max

/** One render worker owns the AvatarForge engine, renderer and ANativeWindow lifetime. */
class AvatarForgeSurfaceView(
    context: Context,
    private val glbFile: File,
    private val onStatus: (String) -> Unit
) : SurfaceView(context), SurfaceHolder.Callback {
    private val running = AtomicBoolean(false)
    @Volatile private var surfaceReady = false
    @Volatile private var surfaceWidth = 0
    @Volatile private var surfaceHeight = 0
    @Volatile private var renderThread: Thread? = null

    init {
        holder.addCallback(this)
        setBackgroundColor(android.graphics.Color.rgb(18, 20, 25))
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceReady = true
        startRenderer(holder)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        surfaceWidth = width
        surfaceHeight = height
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        surfaceReady = false
        stopRenderer()
    }

    private fun startRenderer(surfaceHolder: SurfaceHolder) {
        if (!running.compareAndSet(false, true)) return
        val worker = Thread({
            var nativeHandle = 0L
            try {
                onStatus("Starting AvatarForge GLES renderer and importing ${glbFile.name}…")
                while (running.get() && surfaceReady && (surfaceWidth <= 0 || surfaceHeight <= 0)) Thread.sleep(10)
                if (!running.get() || !surfaceReady) return@Thread
                check(glbFile.isFile && glbFile.length() > 0L) { "Selected GLB is not available in app storage." }
                nativeHandle = nativeCreate(surfaceHolder.surface, glbFile.absolutePath, max(1, surfaceWidth), max(1, surfaceHeight))
                check(nativeHandle != 0L) { "AvatarForge native renderer returned a null session." }
                onStatus("AvatarForge GLES renderer active · ${glbFile.name}")
                var lastFrame = System.nanoTime()
                var lastWidth = surfaceWidth
                var lastHeight = surfaceHeight
                while (running.get() && surfaceReady) {
                    val width = surfaceWidth
                    val height = surfaceHeight
                    if (width > 0 && height > 0 && (width != lastWidth || height != lastHeight)) {
                        nativeResize(nativeHandle, width, height)
                        lastWidth = width
                        lastHeight = height
                    }
                    val now = System.nanoTime()
                    val delta = ((now - lastFrame).toDouble() / 1_000_000_000.0).coerceIn(0.001, 0.1).toFloat()
                    lastFrame = now
                    nativeRender(nativeHandle, delta)
                    val remaining = 16_666_667L - (System.nanoTime() - now)
                    if (remaining > 0L) Thread.sleep(remaining / 1_000_000L, (remaining % 1_000_000L).toInt())
                }
            } catch (failure: Throwable) {
                onStatus("AvatarForge renderer failed: ${failure.message ?: failure.javaClass.simpleName}")
            } finally {
                if (nativeHandle != 0L) runCatching { nativeDestroy(nativeHandle) }
                    .onFailure { onStatus("AvatarForge cleanup failed: ${it.message}") }
                running.set(false)
                onStatus(if (surfaceReady) "AvatarForge renderer stopped." else "AvatarForge surface released.")
            }
        }, "AvatarForge-GLES-Render")
        worker.priority = Thread.NORM_PRIORITY
        renderThread = worker
        worker.start()
    }

    private fun stopRenderer() {
        running.set(false)
        val worker = renderThread
        if (worker != null && worker !== Thread.currentThread()) {
            worker.join() // Never allow a new surface session to overlap a live native renderer
        }
        renderThread = null
    }

    override fun onDetachedFromWindow() {
        stopRenderer()
        holder.removeCallback(this)
        super.onDetachedFromWindow()
    }

    private external fun nativeCreate(surface: android.view.Surface, glbPath: String, width: Int, height: Int): Long
    private external fun nativeResize(handle: Long, width: Int, height: Int)
    private external fun nativeRender(handle: Long, deltaSeconds: Float)
    private external fun nativeDestroy(handle: Long)

    companion object {
        init { System.loadLibrary("mega_hub_avatarforge") }
    }
}
