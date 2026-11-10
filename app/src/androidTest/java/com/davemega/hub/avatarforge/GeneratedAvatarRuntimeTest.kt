package com.davemega.hub.avatarforge

import android.graphics.Bitmap
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.view.SurfaceHolder
import android.view.SurfaceView
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

@RunWith(AndroidJUnit4::class)
class GeneratedAvatarRuntimeTest {
    private external fun nativeCreateGenerated(surface: android.view.Surface, width: Int, height: Int): Long
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
            handle = nativeCreateGenerated(view.holder.surface, surfaceWidth, surfaceHeight)
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

            val bitmap = Bitmap.createBitmap(surfaceWidth, surfaceHeight, Bitmap.Config.ARGB_8888)
            capturedBitmap = bitmap
            val copyLatch = CountDownLatch(1)
            instrumentation.runOnMainSync {
                PixelCopy.request(view, bitmap, { result ->
                    pixelCopyResult.set(result)
                    copyLatch.countDown()
                }, Handler(Looper.getMainLooper()))
            }
            assertTrue("PixelCopy callback timed out", copyLatch.await(15, TimeUnit.SECONDS))

            // Preserve the raw compositor capture before any fallback can replace it.
            val outputDir = requireNotNull(context.getExternalFilesDir(null)) {
                "External app files directory is unavailable"
            }
            val pixelCopyRawFile = File(outputDir, "pixelcopy_raw_capture.png")
            FileOutputStream(pixelCopyRawFile).use { stream ->
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream)
            }
            val pixelCopyImageDiagnostics = bitmapDiagnostics(bitmap)

            var pixelCopyGeometryPixels = 0
            if (pixelCopyResult.get() == PixelCopy.SUCCESS) {
                pixelCopyGeometryPixels = countGeometryPixels(bitmap)
            }

            var acceptedCapture = if (pixelCopyResult.get() == PixelCopy.SUCCESS && pixelCopyGeometryPixels > 5000) {
                "PixelCopy"
            } else {
                "none"
            }
            var acceptedPixelCount = if (acceptedCapture == "PixelCopy") pixelCopyGeometryPixels else 0
            var glReadbackGeometryPixels = 0
            var glReadbackImageDiagnostics = "glReadback=not-requested"
            var glReadbackRawPath = "UNAVAILABLE"
            if (acceptedCapture == "none") {
                val rgba = nativeGetReadbackPixels(handle)
                if (rgba != null && rgba.size == surfaceWidth * surfaceHeight * 4) {
                    val fallback = bitmapFromGlRgba(rgba, surfaceWidth, surfaceHeight)
                    glReadbackGeometryPixels = countGeometryPixels(fallback)
                    glReadbackImageDiagnostics = bitmapDiagnostics(fallback)
                    val glReadbackFile = File(outputDir, "gl_readback_raw_capture.png")
                    FileOutputStream(glReadbackFile).use { stream ->
                        fallback.compress(Bitmap.CompressFormat.PNG, 100, stream)
                    }
                    glReadbackRawPath = glReadbackFile.absolutePath
                    if (glReadbackGeometryPixels > 5000) {
                        acceptedCapture = "glReadPixels"
                        acceptedPixelCount = glReadbackGeometryPixels
                        capturedBitmap?.recycle()
                        capturedBitmap = fallback
                    } else {
                        acceptedCapture = "PixelCopy+glReadPixels-below-threshold"
                        fallback.recycle()
                    }
                } else {
                    acceptedCapture = "PixelCopy-failed+glReadPixels-unavailable"
                    glReadbackImageDiagnostics = "glReadbackBytes=${rgba?.size ?: -1} expectedBytes=${surfaceWidth * surfaceHeight * 4}"
                }
            } else {
                glReadbackGeometryPixels = stats[7]
                glReadbackImageDiagnostics = "nativeStatsReadbackPixels=${stats[7]}"
            }

            val report = nativeGetVerificationReport(handle)
            val totalPixels = surfaceWidth * surfaceHeight
            val acceptedPixelPercent = if (totalPixels > 0) acceptedPixelCount * 100.0f / totalPixels else 0.0f
            val diagnostics = "capture=$acceptedCapture pixelCopyResult=${pixelCopyResult.get()} " +
                "pixelCopyGeometryPixels=$pixelCopyGeometryPixels acceptedGeometryPixels=$acceptedPixelCount " +
                "acceptedPercent=${acceptedPixelPercent}% bitmap=${surfaceWidth}x${surfaceHeight} totalPixels=$totalPixels " +
                "pixelCopyRaw=${pixelCopyRawFile.absolutePath} pixelCopyImage={$pixelCopyImageDiagnostics} " +
                "glReadbackRaw=$glReadbackRawPath glReadbackImage={$glReadbackImageDiagnostics} " +
                "frameCount=${stats[0]} successfulPresentedFrames=${stats[12]} drawCalls=${stats[1]} triangles=${stats[2]} " +
                "vertices=${stats[3]} nativeReadbackGeometryPixels=${stats[7]} " +
                "joints=${stats[10]} blendshapes=${stats[11]} $report"

            if (acceptedPixelCount <= 5000) {
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
            assertTrue("Visible pixel threshold not met. $diagnostics", acceptedPixelCount > 5000)
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
            capturedBitmap?.recycle()
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
