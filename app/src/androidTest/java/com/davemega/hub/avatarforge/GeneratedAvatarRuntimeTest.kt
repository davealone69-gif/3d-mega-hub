package com.davemega.hub.avatarforge

import android.app.Instrumentation
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Rect
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.TextureView
import android.view.ViewGroup
import android.widget.FrameLayout
import android.view.PixelCopy
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.davemega.hub.MainActivity
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import org.json.JSONArray
import org.json.JSONObject

private const val VISIBILITY_THRESHOLD = 5000

private data class CaptureDiagnostics(
    val source: String,
    val pixelCopyStatus: Int?,
    val captureTimestampElapsedRealtimeNanos: Long,
    val rendererStatsTimestampElapsedRealtimeNanos: Long,
    val rendererFrameIndex: Int,
    val rendererPresentedFrames: Int,
    val alphaIndicatorsValid: Boolean,
    val width: Int,
    val height: Int,
    val config: String,
    val hasAlphaChannel: Boolean,
    val alphaZeroFraction: Float,
    val alphaOpaqueFraction: Float,
    val alphaDistinctLevels: Int,
    val alphaHistogram: List<Int>,
    val meanR: Int,
    val meanG: Int,
    val meanB: Int,
    val meanA: Int,
    val stddevA: Float,
    val geometryPixels: Int
)

private data class CaptureResult(
    val bitmap: Bitmap?,
    val pixelCopyBitmap: Bitmap?,
    val nativeFallbackBitmap: Bitmap?,
    val finalDiagnostics: CaptureDiagnostics,
    val pixelCopyDiagnostics: CaptureDiagnostics,
    val nativeFallbackDiagnostics: CaptureDiagnostics?,
    val usedFallback: Boolean
)

@RunWith(AndroidJUnit4::class)
class GeneratedAvatarRuntimeTest {
    private external fun nativeCreateGenerated(surface: android.view.Surface, width: Int, height: Int): Long
    private external fun nativeCreateGeneratedDebug(
        surface: android.view.Surface,
        width: Int,
        height: Int,
        debugMagenta: Boolean
    ): Long
    private external fun nativeRenderGenerated(handle: Long, deltaSeconds: Float): Boolean
    private external fun nativeGetVerificationStats(handle: Long): IntArray
    private external fun nativeGetVerificationReport(handle: Long): String
    private external fun nativeGetReadbackPixels(handle: Long): ByteArray?
    private external fun nativeDestroyGenerated(handle: Long)


    @Test(timeout = 600000)
    fun generatedAvatarDebugShaderMustProduceMagentaPixels() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val scenario = ActivityScenario.launch(MainActivity::class.java)
        val surfaceReady = CountDownLatch(1)
        var surfaceView: SurfaceView? = null
        var surfaceWidth = 0
        var surfaceHeight = 0
        var handle = 0L
        var bitmap: Bitmap? = null

        try {
            scenario.onActivity { activity ->
                val root = activity.findViewById<ViewGroup>(android.R.id.content)
                val view = SurfaceView(activity)
                view.holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(holder: SurfaceHolder) = Unit

                    override fun surfaceChanged(
                        holder: SurfaceHolder,
                        format: Int,
                        width: Int,
                        height: Int
                    ) {
                        if (holder.surface.isValid && width > 0 && height > 0) {
                            surfaceWidth = width
                            surfaceHeight = height
                            surfaceReady.countDown()
                        }
                    }

                    override fun surfaceDestroyed(holder: SurfaceHolder) = Unit
                })
                root.addView(
                    view,
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT
                    )
                )
                surfaceView = view
            }

            assertTrue(
                "Diagnostic SurfaceView did not become ready",
                surfaceReady.await(30, TimeUnit.SECONDS)
            )

            val view = requireNotNull(surfaceView)
            assertTrue("Diagnostic Surface is invalid", view.holder.surface.isValid)

            // The native JNI implementation fixes the generated avatar seed at 1337.
            handle = nativeCreateGeneratedDebug(view.holder.surface, surfaceWidth, surfaceHeight, true)
            assertTrue("Seed-1337 native generation failed", handle != 0L)

            val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(30)
            var presented = false
            while (!presented && System.nanoTime() < deadline) {
                presented = nativeRenderGenerated(handle, 1.0f / 60.0f)
                if (!presented) Thread.sleep(16)
            }
            assertTrue(
                "No EGL-presented frame from magenta diagnostic shader: " +
                    nativeGetVerificationReport(handle),
                presented
            )

            val captured = Bitmap.createBitmap(
                surfaceWidth,
                surfaceHeight,
                Bitmap.Config.ARGB_8888
            )
            bitmap = captured
            val copyLatch = CountDownLatch(1)
            val copyResult = AtomicInteger(Int.MIN_VALUE)

            instrumentation.runOnMainSync {
                PixelCopy.request(
                    view,
                    captured,
                    { result ->
                        copyResult.set(result)
                        copyLatch.countDown()
                    },
                    Handler(Looper.getMainLooper())
                )
            }

            assertTrue("Magenta PixelCopy callback timed out", copyLatch.await(15, TimeUnit.SECONDS))
            assertEquals("Magenta PixelCopy failed", PixelCopy.SUCCESS, copyResult.get())

            val pixels = IntArray(surfaceWidth * surfaceHeight)
            captured.getPixels(
                pixels, 0, surfaceWidth, 0, 0, surfaceWidth, surfaceHeight
            )
            val magentaPixels = pixels.count { pixel ->
                Color.red(pixel) > 200 &&
                    Color.green(pixel) < 50 &&
                    Color.blue(pixel) > 200
            }

            assertTrue(
                "Diagnostic Failed: Magenta shader is not visible. The camera is broken.",
                magentaPixels > 5000
            )
        } finally {
            if (handle != 0L) {
                runCatching { nativeDestroyGenerated(handle) }
            }
            scenario.onActivity { activity ->
                surfaceView?.let { view ->
                    (view.parent as? ViewGroup)?.removeView(view)
                }
            }
            scenario.close()
            bitmap?.recycle()
        }
    }

    @Test(timeout = 600000)
    fun generatedAvatarTextureViewWindowCaptureA_B() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val scenario = ActivityScenario.launch(MainActivity::class.java)
        val surfaceReady = CountDownLatch(1)
        var textureView: TextureView? = null
        var activityWindow: android.view.Window? = null
        var renderSurface: Surface? = null
        var surfaceWidth = 0
        var surfaceHeight = 0
        var handle = 0L
        var bitmap: Bitmap? = null
        try {
            scenario.onActivity { activity ->
                activityWindow = activity.window
                val root = activity.findViewById<ViewGroup>(android.R.id.content)
                val view = TextureView(activity)
                view.surfaceTextureListener = object : TextureView.SurfaceTextureListener {
                    override fun onSurfaceTextureAvailable(surfaceTexture: android.graphics.SurfaceTexture, width: Int, height: Int) {
                        surfaceWidth = width
                        surfaceHeight = height
                        surfaceTexture.setDefaultBufferSize(width, height)
                        renderSurface = Surface(surfaceTexture)
                        surfaceReady.countDown()
                    }
                    override fun onSurfaceTextureSizeChanged(surface: android.graphics.SurfaceTexture, width: Int, height: Int) {
                        surfaceWidth = width
                        surfaceHeight = height
                    }
                    override fun onSurfaceTextureDestroyed(surface: android.graphics.SurfaceTexture): Boolean = true
                    override fun onSurfaceTextureUpdated(surface: android.graphics.SurfaceTexture) = Unit
                }
                root.addView(view, FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                ))
                textureView = view
            }
            assertTrue("TextureView SurfaceTexture did not become ready", surfaceReady.await(30, TimeUnit.SECONDS))
            val surface = requireNotNull(renderSurface)
            assertTrue("TextureView render Surface is invalid", surface.isValid)
            handle = nativeCreateGenerated(surface, surfaceWidth, surfaceHeight)
            assertTrue("TextureView generated-avatar session returned null", handle != 0L)

            val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(30)
            var presented = false
            while (!presented && System.nanoTime() < deadline) {
                presented = nativeRenderGenerated(handle, 1.0f / 60.0f)
                if (!presented) Thread.sleep(16)
            }
            assertTrue("TextureView path did not present an EGL frame: ${nativeGetVerificationReport(handle)}", presented)

            val statsTimestamp = SystemClock.elapsedRealtimeNanos()
            val stats = nativeGetVerificationStats(handle)
            val report = nativeGetVerificationReport(handle)
            val view = requireNotNull(textureView)
            val location = IntArray(2)
            view.getLocationInWindow(location)
            val sourceRect = Rect(
                location[0], location[1],
                location[0] + surfaceWidth, location[1] + surfaceHeight
            )
            val captured = Bitmap.createBitmap(surfaceWidth, surfaceHeight, Bitmap.Config.ARGB_8888)
            bitmap = captured
            val captureTimestamp = SystemClock.elapsedRealtimeNanos()
            val latch = CountDownLatch(1)
            val result = AtomicInteger(Int.MIN_VALUE)
            instrumentation.runOnMainSync {
                PixelCopy.request(
                    requireNotNull(activityWindow),
                    sourceRect,
                    captured,
                    { status -> result.set(status); latch.countDown() },
                    Handler(Looper.getMainLooper())
                )
            }
            assertTrue("TextureView window PixelCopy callback timed out", latch.await(15, TimeUnit.SECONDS))
            assertEquals("TextureView window PixelCopy failed", PixelCopy.SUCCESS, result.get())

            val diag = diagnose(
                captured, "textureview-window", result.get(), captureTimestamp,
                statsTimestamp, stats.getOrNull(0) ?: -1, stats.getOrNull(12) ?: -1
            )
            val outputDir = requireNotNull(context.getExternalFilesDir(null))
            savePng(captured, File(outputDir, "raw_textureview_window.png"))
            saveDiagnosticsJson(File(outputDir, "textureview_capture_diagnostics.json"), listOf(diag))
            assertTrue(
                "TextureView A/B capture did not cross visibility threshold. diag=$diag " +
                    "rendererStats=${stats.contentToString()} rendererReport=$report",
                diag.geometryPixels >= VISIBILITY_THRESHOLD
            )
        } finally {
            if (handle != 0L) runCatching { nativeDestroyGenerated(handle) }
            renderSurface?.release()
            scenario.onActivity { activity ->
                textureView?.let { view -> (view.parent as? ViewGroup)?.removeView(view) }
            }
            scenario.close()
            bitmap?.takeIf { !it.isRecycled }?.recycle()
        }
    }

    @Test(timeout = 600000)
    fun generatedAvatarMustProduceVisiblePixels() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val scenario = ActivityScenario.launch(MainActivity::class.java)
        val surfaceReady = CountDownLatch(1)
        var surfaceView: SurfaceView? = null
        var surfaceWidth = 0
        var surfaceHeight = 0
        var handle = 0L
        var capturedBitmap: Bitmap? = null
        var captureEvidence: CaptureResult? = null
        var failureScreenshotPath: String? = null
        var pixelCopyResult = AtomicInteger(Int.MIN_VALUE)

        try {
            scenario.onActivity { activity ->
                val root = activity.findViewById<ViewGroup>(android.R.id.content)
                val view = SurfaceView(activity)
                view.holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(holder: SurfaceHolder) = Unit
                    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                        if (holder.surface.isValid && width > 0 && height > 0) {
                            surfaceWidth = width
                            surfaceHeight = height
                            surfaceReady.countDown()
                        }
                    }
                    override fun surfaceDestroyed(holder: SurfaceHolder) = Unit
                })
                root.addView(view, FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                ))
                surfaceView = view
            }

            assertTrue("SurfaceView did not become ready within 30 seconds", surfaceReady.await(30, TimeUnit.SECONDS))
            val view = requireNotNull(surfaceView)
            assertTrue("Surface is invalid after surfaceChanged", view.holder.surface.isValid)
            assertTrue("Surface width is zero", surfaceWidth > 0)
            assertTrue("Surface height is zero", surfaceHeight > 0)

            // Generation, renderer creation and every GL call stay on this test worker thread.
            handle = nativeCreateGenerated(view.holder.surface, surfaceWidth, surfaceHeight)
            assertTrue("Native generated-avatar session returned null", handle != 0L)

            val framePresented = CountDownLatch(1)
            val frameDeadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(30)
            while (framePresented.count == 1L && System.nanoTime() < frameDeadline) {
                if (nativeRenderGenerated(handle, 1.0f / 60.0f)) {
                    framePresented.countDown()
                } else {
                    Thread.sleep(16)
                }
            }
            assertTrue("No successful EGL-presented frame within 30 seconds: ${nativeGetVerificationReport(handle)}",
                framePresented.await(1, TimeUnit.SECONDS))

            val stats = nativeGetVerificationStats(handle)
            assertEquals("Native verification stats must contain all fields", 16, stats.size)
            assertTrue("Renderer reported no draw calls: ${nativeGetVerificationReport(handle)}", stats[1] > 0)
            val generatedMetricsReport = nativeGetVerificationReport(handle)
            fun generatedMetric(name: String): Int =
                Regex("""\b${Regex.escape(name)}=(\d+)\b""")
                    .find(generatedMetricsReport)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0
            val generatedTriangles = generatedMetric("triangles")
            val generatedVertices = generatedMetric("vertices")
            val generatedJoints = generatedMetric("joints")
            val generatedBlendshapes = generatedMetric("blendshapes")
            assertTrue("Generated engine reported no triangles: $generatedMetricsReport", generatedTriangles > 0)
            assertTrue("Generated engine reported no vertices: $generatedMetricsReport", generatedVertices > 0)
            assertTrue("Generated engine reported no joints: $generatedMetricsReport", generatedJoints > 0)
            assertTrue("Generated engine reported no blendshapes: $generatedMetricsReport", generatedBlendshapes > 0)
            assertEquals("Renderer triangle count differs from the live generated engine", generatedTriangles, stats[2])
            assertEquals("Renderer vertex count differs from the live generated engine", generatedVertices, stats[3])
            assertEquals("Renderer joint count differs from the live generated engine", generatedJoints, stats[10])
            assertEquals("Renderer blendshape count differs from the live generated engine", generatedBlendshapes, stats[11])
            assertTrue("No successfully presented frames were recorded: ${nativeGetVerificationReport(handle)}", stats[12] > 0)
            assertTrue("Not all eight AABB corners are within the camera depth interval: ${nativeGetVerificationReport(handle)}", stats[13] == 1)
            assertEquals("Camera-fit bounds must keep all eight model AABB corners inside the clip volume",
                0, stats[15])
            assertEquals("No AABB corner should be reported outside the clip volume", -1, stats[14])
            assertTrue("Deformed AABB does not intersect the actual camera frustum: ${nativeGetVerificationReport(handle)}",
                stats[5] == 1)
            assertTrue("GL framebuffer readback failed: ${nativeGetVerificationReport(handle)}", stats[6] == 1)
            assertEquals("GL readback width differs from the live SurfaceView", surfaceWidth, stats[8])
            assertEquals("GL readback height differs from the live SurfaceView", surfaceHeight, stats[9])

            val rendererStatsTimestampNanos = SystemClock.elapsedRealtimeNanos()
            val rendererStatsAtCapture = nativeGetVerificationStats(handle)
            val rendererReportAtCapture = nativeGetVerificationReport(handle)
            val capture = captureWithEvidence(
                context = context,
                view = view,
                width = surfaceWidth,
                height = surfaceHeight,
                handle = handle,
                rendererStatsAtCapture = rendererStatsAtCapture,
                rendererStatsTimestampNanos = rendererStatsTimestampNanos,
                pixelCopyStatus = pixelCopyResult,
                instrumentation = instrumentation
            )
            captureEvidence = capture
            capturedBitmap = capture.bitmap
            val statsForReport = nativeGetVerificationStats(handle)
            val report = nativeGetVerificationReport(handle)
            val totalPixels = surfaceWidth * surfaceHeight
            val finalDiag = capture.finalDiagnostics
            val pixelCopyDiag = capture.pixelCopyDiagnostics
            val nativeDiag = capture.nativeFallbackDiagnostics
            val diagnostics = "capture=${finalDiag.source} usedFallback=${capture.usedFallback} " +
                "pixelCopyStatus=${pixelCopyResult.get()} captureTimestampNs=${finalDiag.captureTimestampElapsedRealtimeNanos} " +
                "rendererFrameAtCapture=${finalDiag.rendererFrameIndex} presentedAtCapture=${finalDiag.rendererPresentedFrames} " +
                "pixelCopy=${pixelCopyDiag} nativeFallback=${nativeDiag ?: "not-run"} " +
                "final=${finalDiag} bitmap=${surfaceWidth}x${surfaceHeight} totalPixels=$totalPixels " +
                "frameCount=${statsForReport[0]} successfulPresentedFrames=${statsForReport[12]} " +
                "drawCalls=${statsForReport[1]} triangles=${statsForReport[2]} vertices=${statsForReport[3]} " +
                "nativeReadbackGeometryPixels=${statsForReport[7]} joints=${statsForReport[10]} " +
                "blendshapes=${statsForReport[11]} rendererReportAtCapture=[$rendererReportAtCapture] finalReport=[$report]"

            if (capture.usedFallback &&
                pixelCopyDiag.geometryPixels < VISIBILITY_THRESHOLD &&
                finalDiag.geometryPixels >= VISIBILITY_THRESHOLD
            ) {
                Log.w(
                    "GeneratedAvatarRuntimeTest",
                    "CAPTURE_PIPELINE_REGRESSION: native fallback passed while PixelCopy failed visibility. " +
                        "pixelCopy=$pixelCopyDiag nativeFallback=$nativeDiag"
                )
            }

            if (finalDiag.geometryPixels < VISIBILITY_THRESHOLD) {
                val outputDir = requireNotNull(context.getExternalFilesDir(null)) {
                    "External app files directory is unavailable"
                }
                val screenshot = File(outputDir, "failed_render.png")
                FileOutputStream(screenshot).use { stream ->
                    requireNotNull(capturedBitmap).compress(Bitmap.CompressFormat.PNG, 100, stream)
                }
                failureScreenshotPath = screenshot.absolutePath
                throw AssertionError("Generated avatar is not visibly rendered. $diagnostics screenshot=${screenshot.absolutePath}")
            }
            assertTrue("Visible pixel threshold not met. $diagnostics", finalDiag.geometryPixels >= VISIBILITY_THRESHOLD)
        } catch (failure: Throwable) {
            val view = surfaceView
            if (failureScreenshotPath == null && view != null && view.width > 0 && view.height > 0) {
                if (capturedBitmap == null) {
                    val failureBitmap = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
                    val failureCopy = CountDownLatch(1)
                    val failureCopyResult = AtomicInteger(Int.MIN_VALUE)
                    instrumentation.runOnMainSync {
                        PixelCopy.request(view, failureBitmap, { result ->
                            failureCopyResult.set(result)
                            failureCopy.countDown()
                        }, Handler(Looper.getMainLooper()))
                    }
                    if (failureCopy.await(5, TimeUnit.SECONDS) && failureCopyResult.get() == PixelCopy.SUCCESS) {
                        capturedBitmap = failureBitmap
                    } else {
                        failureBitmap.recycle()
                    }
                }
                if (capturedBitmap == null && handle != 0L) {
                    val rgba = runCatching { nativeGetReadbackPixels(handle) }.getOrNull()
                    if (rgba != null && rgba.size == view.width * view.height * 4) {
                        capturedBitmap = bitmapFromGlRgba(rgba, view.width, view.height)
                    }
                }
                capturedBitmap?.let { image ->
                    val outputDir = context.getExternalFilesDir(null)
                    if (outputDir != null) {
                        val screenshot = File(outputDir, "failed_render.png")
                        runCatching {
                            FileOutputStream(screenshot).use { stream ->
                                image.compress(Bitmap.CompressFormat.PNG, 100, stream)
                            }
                        }.onSuccess { failureScreenshotPath = screenshot.absolutePath }
                    }
                }
            }
            val suffix = failureScreenshotPath?.let { " failed_render.png=$it" } ?: " failed_render.png=UNAVAILABLE"
            throw AssertionError("${failure.message ?: failure.javaClass.simpleName}.$suffix", failure)
        } finally {
            if (handle != 0L) {
                runCatching { nativeDestroyGenerated(handle) }
            }
            scenario.onActivity { activity ->
                surfaceView?.let { view -> (view.parent as? ViewGroup)?.removeView(view) }
            }
            scenario.close()
            listOfNotNull(captureEvidence?.pixelCopyBitmap, captureEvidence?.nativeFallbackBitmap).distinct().forEach { bitmap ->
                if (!bitmap.isRecycled) bitmap.recycle()
            }
            if (captureEvidence == null) capturedBitmap?.takeIf { !it.isRecycled }?.recycle()
        }
    }

    private fun captureWithEvidence(
        context: android.content.Context,
        view: SurfaceView,
        width: Int,
        height: Int,
        handle: Long,
        rendererStatsAtCapture: IntArray,
        rendererStatsTimestampNanos: Long,
        pixelCopyStatus: AtomicInteger,
        instrumentation: Instrumentation
    ): CaptureResult {
        val outputDir = requireNotNull(context.getExternalFilesDir(null)) {
            "External app files directory is unavailable"
        }

        // Primary capture is immutable evidence. Never recycle or reassign it during fallback.
        // Retain the allocated bitmap even when PixelCopy reports an error or times out:
        // its contents are still useful evidence of what the attempted capture produced.
        val pixelCopyBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val captureTimestampNanos = SystemClock.elapsedRealtimeNanos()
        val rendererFrameIndex = rendererStatsAtCapture.getOrNull(0) ?: -1
        val rendererPresentedFrames = rendererStatsAtCapture.getOrNull(12) ?: -1
        val copyLatch = CountDownLatch(1)
        runCatching {
            instrumentation.runOnMainSync {
                PixelCopy.request(view, pixelCopyBitmap, { result ->
                    pixelCopyStatus.set(result)
                    copyLatch.countDown()
                }, Handler(Looper.getMainLooper()))
            }
        }.onFailure {
            pixelCopyStatus.compareAndSet(Int.MIN_VALUE, -1)
        }
        val pixelCopyCompleted = runCatching { copyLatch.await(15, TimeUnit.SECONDS) }.getOrDefault(false)
        if (!pixelCopyCompleted) pixelCopyStatus.compareAndSet(Int.MIN_VALUE, -2)

        val pixelCopyDiagnostics = diagnose(
            pixelCopyBitmap, "pixelcopy", pixelCopyStatus.get(), captureTimestampNanos,
            rendererStatsTimestampNanos, rendererFrameIndex, rendererPresentedFrames
        )
        pixelCopyBitmap?.let { savePng(it, File(outputDir, "raw_pixelcopy.png")) }

        var finalBitmap = pixelCopyBitmap
        var finalDiagnostics = pixelCopyDiagnostics
        var fallbackBitmap: Bitmap? = null
        var fallbackDiagnostics: CaptureDiagnostics? = null
        var usedFallback = false

        if (pixelCopyDiagnostics.geometryPixels < VISIBILITY_THRESHOLD) {
            fallbackBitmap = runCatching {
                val rgba = nativeGetReadbackPixels(handle)
                check(rgba != null && rgba.size == width * height * 4) {
                    "Native readback unavailable: bytes=${rgba?.size ?: -1}, expected=${width * height * 4}"
                }
                bitmapFromGlRgba(requireNotNull(rgba), width, height)
            }.getOrNull()
            fallbackDiagnostics = diagnose(
                fallbackBitmap, "native-fallback", null, SystemClock.elapsedRealtimeNanos(),
                rendererStatsTimestampNanos, rendererFrameIndex, rendererPresentedFrames
            )
            fallbackBitmap?.let { savePng(it, File(outputDir, "raw_native_fallback.png")) }

            if (fallbackBitmap != null &&
                fallbackDiagnostics.geometryPixels >= VISIBILITY_THRESHOLD
            ) {
                finalBitmap = fallbackBitmap
                finalDiagnostics = fallbackDiagnostics
                usedFallback = true
            }
        }

        saveDiagnosticsJson(
            File(outputDir, "capture_diagnostics.json"),
            listOfNotNull(pixelCopyDiagnostics, fallbackDiagnostics)
        )

        // failed_render is the chosen attempted frame, while raw captures remain untouched.
        finalBitmap?.let { savePng(it, File(outputDir, "failed_render.png")) }
        return CaptureResult(
            bitmap = finalBitmap,
            pixelCopyBitmap = pixelCopyBitmap,
            nativeFallbackBitmap = fallbackBitmap,
            finalDiagnostics = finalDiagnostics,
            pixelCopyDiagnostics = pixelCopyDiagnostics,
            nativeFallbackDiagnostics = fallbackDiagnostics,
            usedFallback = usedFallback
        )
    }

    private fun diagnose(
        bitmap: Bitmap?,
        source: String,
        pixelCopyStatus: Int?,
        captureTimestampElapsedRealtimeNanos: Long,
        rendererStatsTimestampElapsedRealtimeNanos: Long,
        rendererFrameIndex: Int,
        rendererPresentedFrames: Int
    ): CaptureDiagnostics {
        if (bitmap == null) {
            return CaptureDiagnostics(
                source, pixelCopyStatus, captureTimestampElapsedRealtimeNanos,
                rendererStatsTimestampElapsedRealtimeNanos, rendererFrameIndex, rendererPresentedFrames, false,
                0, 0, "null", false,
                1f, 0f, 0, List(256) { 0 }, 0, 0, 0, 0, 0f, 0
            )
        }
        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        val alphaHistogram = IntArray(256)
        var sumR = 0L
        var sumG = 0L
        var sumB = 0L
        var sumA = 0L
        for (pixel in pixels) {
            val r = Color.red(pixel)
            val g = Color.green(pixel)
            val b = Color.blue(pixel)
            val a = Color.alpha(pixel)
            sumR += r
            sumG += g
            sumB += b
            sumA += a
            alphaHistogram[a]++
        }
        check(alphaHistogram.sum() == width * height) {
            "Alpha histogram invariant failed for $source: ${alphaHistogram.sum()} != ${width * height}"
        }
        val count = pixels.size.coerceAtLeast(1)
        val meanA = sumA.toDouble() / count
        var alphaSquaredError = 0.0
        for (alpha in 0..255) {
            val delta = alpha - meanA
            alphaSquaredError += delta * delta * alphaHistogram[alpha]
        }
        val zeroAlpha = alphaHistogram[0]
        val opaqueAlpha = alphaHistogram[255]
        return CaptureDiagnostics(
            source = source,
            pixelCopyStatus = pixelCopyStatus,
            captureTimestampElapsedRealtimeNanos = captureTimestampElapsedRealtimeNanos,
            rendererStatsTimestampElapsedRealtimeNanos = rendererStatsTimestampElapsedRealtimeNanos,
            rendererFrameIndex = rendererFrameIndex,
            rendererPresentedFrames = rendererPresentedFrames,
            alphaIndicatorsValid = bitmap.hasAlpha(),
            width = width,
            height = height,
            config = bitmap.config?.name ?: "unknown",
            hasAlphaChannel = bitmap.hasAlpha(),
            alphaZeroFraction = zeroAlpha.toFloat() / count,
            alphaOpaqueFraction = opaqueAlpha.toFloat() / count,
            alphaDistinctLevels = alphaHistogram.count { it > 0 },
            alphaHistogram = alphaHistogram.toList(),
            meanR = (sumR / count).toInt(),
            meanG = (sumG / count).toInt(),
            meanB = (sumB / count).toInt(),
            meanA = meanA.toInt(),
            stddevA = kotlin.math.sqrt(alphaSquaredError / count).toFloat(),
            geometryPixels = countGeometryPixels(bitmap)
        )
    }

    private fun savePng(bitmap: Bitmap, file: File) {
        FileOutputStream(file).use { stream ->
            check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream)) {
                "PNG compression failed for ${file.absolutePath}"
            }
            stream.flush()
        }
    }

    private fun saveDiagnosticsJson(file: File, diagnostics: List<CaptureDiagnostics>) {
        val json = JSONArray()
        diagnostics.forEach { item ->
            json.put(JSONObject().apply {
                put("source", item.source)
                put("width", item.width)
                put("height", item.height)
                put("config", item.config)
                put("hasAlphaChannel", item.hasAlphaChannel)
                put("alphaZeroFraction", item.alphaZeroFraction.toDouble())
                put("alphaOpaqueFraction", item.alphaOpaqueFraction.toDouble())
                put("alphaDistinctLevels", item.alphaDistinctLevels)
                put("alphaHistogram", JSONArray(item.alphaHistogram))
                put("meanR", item.meanR)
                put("meanG", item.meanG)
                put("meanB", item.meanB)
                put("meanA", item.meanA)
                put("stddevA", item.stddevA.toDouble())
                put("geometryPixels", item.geometryPixels)
            })
        }
        FileOutputStream(file).use { stream ->
            stream.write(json.toString(2).toByteArray(Charsets.UTF_8))
            stream.flush()
        }
    }

    private fun bitmapDiagnostics(bitmap: Bitmap): String {
        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        var minAlpha = 255
        var maxAlpha = 0
        var minRed = 255
        var maxRed = 0
        var minGreen = 255
        var maxGreen = 0
        var minBlue = 255
        var maxBlue = 0
        var transparent = 0
        for (pixel in pixels) {
            val alpha = Color.alpha(pixel)
            val red = Color.red(pixel)
            val green = Color.green(pixel)
            val blue = Color.blue(pixel)
            minAlpha = minOf(minAlpha, alpha)
            maxAlpha = maxOf(maxAlpha, alpha)
            minRed = minOf(minRed, red)
            maxRed = maxOf(maxRed, red)
            minGreen = minOf(minGreen, green)
            maxGreen = maxOf(maxGreen, green)
            minBlue = minOf(minBlue, blue)
            maxBlue = maxOf(maxBlue, blue)
            if (alpha == 0) transparent++
        }
        return "size=${width}x${height} alpha=[${minAlpha},${maxAlpha}] transparent=$transparent " +
            "red=[${minRed},${maxRed}] green=[${minGreen},${maxGreen}] blue=[${minBlue},${maxBlue}]"
    }

    private fun countGeometryPixels(bitmap: Bitmap): Int {
        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        val corners = intArrayOf(pixels[0], pixels[width - 1], pixels[(height - 1) * width], pixels[pixels.lastIndex])
        fun median(channel: (Int) -> Int): Int {
            val values = corners.map(channel).sorted()
            return (values[1] + values[2]) / 2
        }
        val backgroundR = median(Color::red)
        val backgroundG = median(Color::green)
        val backgroundB = median(Color::blue)
        return pixels.count { color ->
            val dr = kotlin.math.abs(Color.red(color) - backgroundR)
            val dg = kotlin.math.abs(Color.green(color) - backgroundG)
            val db = kotlin.math.abs(Color.blue(color) - backgroundB)
            dr + dg + db >= 48 && maxOf(dr, dg, db) >= 16
        }
    }

    private fun bitmapFromGlRgba(bytes: ByteArray, width: Int, height: Int): Bitmap {
        val colors = IntArray(width * height)
        for (y in 0 until height) {
            val sourceY = height - 1 - y // OpenGL readback origin is bottom-left.
            for (x in 0 until width) {
                val offset = (sourceY * width + x) * 4
                val red = bytes[offset].toInt() and 0xff
                val green = bytes[offset + 1].toInt() and 0xff
                val blue = bytes[offset + 2].toInt() and 0xff
                val alpha = bytes[offset + 3].toInt() and 0xff
                colors[y * width + x] = Color.argb(alpha, red, green, blue)
            }
        }
        return Bitmap.createBitmap(colors, width, height, Bitmap.Config.ARGB_8888)
    }

    companion object {
        init {
            System.loadLibrary("mega_hub_avatarforge")
        }
    }
}

