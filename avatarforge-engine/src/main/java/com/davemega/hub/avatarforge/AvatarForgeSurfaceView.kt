package com.davemega.hub.avatarforge

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.PixelCopy
import android.view.SurfaceHolder
import android.view.SurfaceView
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.math.abs
import kotlin.math.max

/** One render worker owns the AvatarForge engine, renderer and ANativeWindow lifetime. */
class AvatarForgeSurfaceView(
    context: Context,
    private val glbFile: File,
    private val verifyVisiblePixels: Boolean = false,
    private val onStatus: (String) -> Unit
) : SurfaceView(context), SurfaceHolder.Callback {
    private val running = AtomicBoolean(false)
    private val pixelCopyPending = AtomicBoolean(false)
    private val pixelCopyComplete = AtomicBoolean(false)
    private val pixelCopyAttempts = AtomicInteger(0)
    private val pixelCopyActiveAttempt = AtomicInteger(0)
    private val mainHandler = Handler(Looper.getMainLooper())
    @Volatile private var surfaceReady = false
    @Volatile private var surfaceWidth = 0
    @Volatile private var surfaceHeight = 0
    @Volatile private var renderThread: Thread? = null

    init {
        holder.addCallback(this)
        setBackgroundColor(android.graphics.Color.rgb(18, 20, 25))
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceCreated valid=${holder.surface.isValid}")
        surfaceReady = true
        pixelCopyPending.set(false)
        pixelCopyComplete.set(false)
        pixelCopyAttempts.set(0)
        pixelCopyActiveAttempt.set(0)
        startRenderer(holder)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "surfaceChanged format=$format size=${width}x${height}")
        surfaceWidth = width
        surfaceHeight = height
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceDestroyed")
        surfaceReady = false
        pixelCopyActiveAttempt.set(0)
        pixelCopyPending.set(false)
        pixelCopyComplete.set(true)
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
                Log.i(TAG, "nativeCreate GLB=${glbFile.absolutePath} size=${surfaceWidth}x${surfaceHeight}")
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
                    val presentedThisFrame = nativeRender(nativeHandle, delta)
                    val stats = nativeGetStats(nativeHandle)
                    check(stats.size == 9) { "AvatarForge returned ${stats.size} renderer statistics; expected 9." }
                    val frames = stats[0]
                    val drawCalls = stats[1]
                    val triangles = stats[2]
                    val framePresented = stats[3] == 1
                    val presentedFrames = stats[4]
                    val boundsInFrustum = stats[5] == 1
                    val outsideCorners = stats[6]
                    val glError = stats[7]
                    val eglError = stats[8]
                    if (!reportedFrame) {
                        if (presentedThisFrame && framePresented && presentedFrames >= 3 && drawCalls > 0 && triangles > 0 && boundsInFrustum) {
                            reportedFrame = true
                            recordSmokeResult("FRAME_PRESENTED", apiVersion, frames, drawCalls, triangles,
                                presentedFrames, boundsInFrustum, outsideCorners, glError, eglError)
                            Log.i(TAG, "FRAME_PRESENTED api=$apiVersion frames=$frames presented=$presentedFrames draws=$drawCalls triangles=$triangles boundsInFrustum=$boundsInFrustum")
                            onStatus("EGL frame presented · $presentedFrames successful swaps · $drawCalls draw calls · $triangles triangles")
                        } else if (System.nanoTime() - frameWaitStarted > 15_000_000_000L) {
                            error("No drawable EGL-presented model frame within 15 seconds " +
                                "(frames=$frames presented=$presentedFrames framePresented=$framePresented " +
                                "draws=$drawCalls triangles=$triangles boundsInFrustum=$boundsInFrustum " +
                                "outsideCorners=$outsideCorners glError=0x${glError.toString(16)} eglError=0x${eglError.toString(16)})")
                        }
                    }
                    if (verifyVisiblePixels && presentedThisFrame && presentedFrames >= 3 &&
                        drawCalls > 0 && triangles > 0 && boundsInFrustum && !pixelCopyComplete.get()) {
                        requestVisiblePixelCopy(apiVersion, frames, drawCalls, triangles, presentedFrames,
                            boundsInFrustum, outsideCorners, glError, eglError)
                    }
                    val remaining = 16_666_667L - (System.nanoTime() - now)
                    if (remaining > 0L) Thread.sleep(remaining / 1_000_000L, (remaining % 1_000_000L).toInt())
                }
            } catch (failure: Throwable) {
                val message = failure.message ?: failure.javaClass.simpleName
                recordSmokeResult("FAILED", error = message)
                Log.e(TAG, "Renderer failed: $message", failure)
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
        presentedFrames: Int = 0,
        boundsInFrustum: Boolean = false,
        outsideCorners: Int = 0,
        glError: Int = 0,
        eglError: Int = 0,
        pixelCount: Int = 0,
        screenshotPath: String = "",
        error: String = ""
    ) {
        context.getSharedPreferences(SMOKE_PREFS, Context.MODE_PRIVATE).edit()
            .putString("status", status)
            .putString("api_version", apiVersion)
            .putInt("frames", frames)
            .putInt("draw_calls", drawCalls)
            .putInt("triangles", triangles)
            .putInt("presented_frames", presentedFrames)
            .putBoolean("bounds_in_frustum", boundsInFrustum)
            .putInt("outside_corners", outsideCorners)
            .putInt("gl_error", glError)
            .putInt("egl_error", eglError)
            .putInt("pixel_count", pixelCount)
            .putString("screenshot_path", screenshotPath)
            .putString("error", error)
            .commit()
    }

    private fun requestVisiblePixelCopy(
        apiVersion: String,
        frames: Int,
        drawCalls: Int,
        triangles: Int,
        presentedFrames: Int,
        boundsInFrustum: Boolean,
        outsideCorners: Int,
        glError: Int,
        eglError: Int
    ) {
        if (!verifyVisiblePixels || pixelCopyComplete.get() ||
            !pixelCopyPending.compareAndSet(false, true)) return
        val attempt = pixelCopyAttempts.incrementAndGet()
        val posted = mainHandler.post {
            if (!surfaceReady || pixelCopyComplete.get()) {
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                return@post
            }
            pixelCopyActiveAttempt.set(attempt)
            val captureWidth = width
            val captureHeight = height
            if (captureWidth <= 0 || captureHeight <= 0 ||
                captureWidth.toLong() * captureHeight.toLong() > Int.MAX_VALUE.toLong()) {
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                if (attempt >= MAX_PIXEL_COPY_ATTEMPTS) {
                    pixelCopyComplete.set(true)
                    val failure = "FAILED: PixelCopy has invalid SurfaceView dimensions after $attempt attempts"
                    recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                        presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = failure)
                    onStatus(failure)
                }
                return@post
            }

            val bitmap = runCatching {
                Bitmap.createBitmap(captureWidth, captureHeight, Bitmap.Config.ARGB_8888)
            }.getOrElse { failure ->
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                pixelCopyComplete.set(true)
                val message = "FAILED: could not allocate PixelCopy bitmap: ${failure.message}"
                recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                    presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                onStatus(message)
                return@post
            }

            val bitmapReleased = AtomicBoolean(false)
            fun recycleBitmap() {
                if (bitmapReleased.compareAndSet(false, true)) bitmap.recycle()
            }
            val timeout = Runnable {
                if (pixelCopyActiveAttempt.compareAndSet(attempt, 0) &&
                    pixelCopyPending.compareAndSet(true, false)) {
                    pixelCopyComplete.set(true)
                    recycleBitmap()
                    val message = "FAILED: PixelCopy callback timed out after ${PIXEL_COPY_TIMEOUT_MS / 1000L}s"
                    recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                        presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                    onStatus(message)
                }
            }
            mainHandler.postDelayed(timeout, PIXEL_COPY_TIMEOUT_MS)

            try {
                PixelCopy.request(this, bitmap, { copyResult ->
                    /* A late callback from a timed-out or destroyed surface must never
                       complete a newer request or overwrite its diagnostic result. */
                    if (pixelCopyActiveAttempt.get() != attempt ||
                        !pixelCopyPending.compareAndSet(true, false)) {
                        recycleBitmap()
                        return@request
                    }
                    mainHandler.removeCallbacks(timeout)
                    pixelCopyActiveAttempt.compareAndSet(attempt, 0)

                    if (copyResult != PixelCopy.SUCCESS) {
                        recycleBitmap()
                        if (attempt >= MAX_PIXEL_COPY_ATTEMPTS) {
                            pixelCopyComplete.set(true)
                            val message = "FAILED: PixelCopy returned result=$copyResult after $attempt attempts"
                            recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                                presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                            onStatus(message)
                        } else {
                            onStatus("PixelCopy attempt $attempt failed (result=$copyResult); retrying after next presented frame")
                        }
                        return@request
                    }

                    val pixels = IntArray(captureWidth * captureHeight)
                    bitmap.getPixels(pixels, 0, captureWidth, 0, 0, captureWidth, captureHeight)
                    /* Estimate the actual clear colour from the four corners. A dark
                       blue-grey background must not count as avatar geometry. */
                    val corners = intArrayOf(
                        pixels[0],
                        pixels[captureWidth - 1],
                        pixels[(captureHeight - 1) * captureWidth],
                        pixels[pixels.lastIndex]
                    )
                    fun medianChannel(channel: (Int) -> Int): Int {
                        val values = corners.map(channel).sorted()
                        return (values[1] + values[2]) / 2
                    }
                    val backgroundRed = medianChannel(Color::red)
                    val backgroundGreen = medianChannel(Color::green)
                    val backgroundBlue = medianChannel(Color::blue)
                    val visiblePixels = pixels.count { pixel ->
                        val redDelta = abs(Color.red(pixel) - backgroundRed)
                        val greenDelta = abs(Color.green(pixel) - backgroundGreen)
                        val blueDelta = abs(Color.blue(pixel) - backgroundBlue)
                        redDelta + greenDelta + blueDelta >= 48 &&
                            maxOf(redDelta, greenDelta, blueDelta) >= 16
                    }
                    val passed = visiblePixels >= MIN_VISIBLE_PIXELS
                    val outputName = if (passed) "generated_avatar_diagnostic.png" else "failed_render.png"
                    val outputFile = context.getExternalFilesDir(null)?.let { File(it, outputName) }
                    val saved = outputFile != null && runCatching {
                        FileOutputStream(outputFile).use { stream ->
                            check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream)) {
                                "Bitmap PNG compression failed"
                            }
                        }
                    }.isSuccess
                    recycleBitmap()
                    pixelCopyComplete.set(true)
                    val screenshot = if (saved) outputFile!!.absolutePath else "UNAVAILABLE"
                    val message = if (passed) {
                        "REAL + VERIFIED: PixelCopy saw $visiblePixels geometry pixels (background RGB=$backgroundRed,$backgroundGreen,$backgroundBlue) after $presentedFrames successful EGL swaps; screenshot=$screenshot"
                    } else {
                        "FAILED: PixelCopy saw only $visiblePixels geometry pixels after background subtraction (required >=$MIN_VISIBLE_PIXELS); screenshot=$screenshot"
                    }
                    recordSmokeResult(if (passed) "PIXELS_VERIFIED" else "FAILED",
                        apiVersion, frames, drawCalls, triangles, presentedFrames, boundsInFrustum,
                        outsideCorners, glError, eglError, visiblePixels, screenshot,
                        if (passed) "" else message)
                    onStatus(message)
                }, mainHandler)
            } catch (failure: Throwable) {
                mainHandler.removeCallbacks(timeout)
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                pixelCopyComplete.set(true)
                recycleBitmap()
                val message = "FAILED: PixelCopy request threw ${failure.message ?: failure.javaClass.simpleName}"
                recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                    presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                onStatus(message)
            }
        }
        if (!posted) {
            pixelCopyActiveAttempt.compareAndSet(attempt, 0)
            pixelCopyPending.set(false)
            pixelCopyComplete.set(true)
            val message = "FAILED: main thread rejected the PixelCopy request"
            recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
            onStatus(message)
        }
    }
    private external fun nativeCreate(surface: android.view.Surface, glbPath: String, width: Int, height: Int): Long
    private external fun nativeResize(handle: Long, width: Int, height: Int)
    private external fun nativeRender(handle: Long, deltaSeconds: Float): Boolean
    private external fun nativeGetInfo(handle: Long): String
    private external fun nativeGetStats(handle: Long): IntArray
    private external fun nativeDestroy(handle: Long)

    companion object {
        private const val TAG = "AvatarForge"
        private const val SMOKE_PREFS = "avatarforge_renderer_smoke"
        private const val MIN_VISIBLE_PIXELS = 5_000
        private const val MAX_PIXEL_COPY_ATTEMPTS = 3
        private const val PIXEL_COPY_TIMEOUT_MS = 15_000L
        init { System.loadLibrary("mega_hub_avatarforge") }
    }
}
