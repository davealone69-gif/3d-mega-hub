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
                recordSmokeResult("STARTING")
                onStatus("Starting AvatarForge GLES renderer and importing ${glbFile.name}…")
                while (running.get() && surfaceReady && (surfaceWidth <= 0 || surfaceHeight <= 0)) Thread.sleep(10)
                if (!running.get() || !surfaceReady) return@Thread
                check(glbFile.isFile && glbFile.length() > 0L) { "Selected GLB is not available in app storage." }
                nativeHandle = nativeCreate(surfaceHolder.surface, glbFile.absolutePath, max(1, surfaceWidth), max(1, surfaceHeight))
                check(nativeHandle != 0L) { "AvatarForge native renderer returned a null session." }
                val rendererInfo = nativeGetInfo(nativeHandle)
                check(rendererInfo.startsWith("gles31|")) {
                    "Unexpected AvatarForge backend information: $rendererInfo"
                }
                val apiVersion = rendererInfo.substringAfter('|').substringBefore('|')
                recordSmokeResult("RENDERER_ACTIVE", apiVersion = apiVersion)
                onStatus("AvatarForge GLES renderer active · $apiVersion · ${glbFile.name}")
                var lastFrame = System.nanoTime()
                var lastWidth = surfaceWidth
                var lastHeight = surfaceHeight
                var reportedFrame = false
                val frameWaitStarted = System.nanoTime()
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
                    if (!reportedFrame) {
                        val stats = nativeGetStats(nativeHandle)
                        check(stats.size == 3) { "AvatarForge returned incomplete renderer statistics." }
                        val frames = stats[0]
                        val drawCalls = stats[1]
                        val triangles = stats[2]
                        if (frames >= 3 && drawCalls > 0 && triangles > 0) {
                            reportedFrame = true
                            recordSmokeResult("FRAME_VERIFIED", apiVersion, frames, drawCalls, triangles)
                            onStatus("AvatarForge GLES frame verified · $frames frames · $drawCalls draw calls · $triangles triangles")
                        } else if (System.nanoTime() - frameWaitStarted > 5_000_000_000L) {
                            error("AvatarForge produced no drawable model frame within 5 seconds (frames=$frames, draws=$drawCalls, triangles=$triangles)")
                        }
                    }
                    val remaining = 16_666_667L - (System.nanoTime() - now)
                    if (remaining > 0L) Thread.sleep(remaining / 1_000_000L, (remaining % 1_000_000L).toInt())
                }
            } catch (failure: Throwable) {
                val message = failure.message ?: failure.javaClass.simpleName
                recordSmokeResult("FAILED", error = message)
                onStatus("AvatarForge renderer failed: $message")
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

    private fun recordSmokeResult(
        status: String,
        apiVersion: String = "",
        frames: Int = 0,
        drawCalls: Int = 0,
        triangles: Int = 0,
        error: String = ""
    ) {
        context.getSharedPreferences(SMOKE_PREFS, Context.MODE_PRIVATE).edit()
            .putString("status", status)
            .putString("api_version", apiVersion)
            .putInt("frames", frames)
            .putInt("draw_calls", drawCalls)
            .putInt("triangles", triangles)
            .putString("error", error)
            .commit()
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
    private external fun nativeGetInfo(handle: Long): String
    private external fun nativeGetStats(handle: Long): IntArray
    private external fun nativeDestroy(handle: Long)

    companion object {
        private const val SMOKE_PREFS = "avatarforge_renderer_smoke"
        init { System.loadLibrary("mega_hub_avatarforge") }
    }
}
