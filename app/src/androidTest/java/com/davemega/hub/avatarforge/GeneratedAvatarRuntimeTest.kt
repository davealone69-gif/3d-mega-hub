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
            assertTrue("Renderer reported no triangles: ${nativeGetVerificationReport(handle)}", stats[2] > 0)
            assertTrue("No successfully presented frames were recorded: ${nativeGetVerificationReport(handle)}", stats[12] > 0)
            assertTrue("Not all eight AABB corners are within the camera depth interval: ${nativeGetVerificationReport(handle)}", stats[13] == 1)
            assertTrue("Deformed AABB does not intersect the actual camera frustum: ${nativeGetVerificationReport(handle)}",
                stats[5] == 1)
            assertTrue("GL framebuffer readback failed: ${nativeGetVerificationReport(handle)}", stats[6] == 1)

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

            var pixelCopyNonBlack = 0
            if (pixelCopyResult.get() == PixelCopy.SUCCESS) {
                pixelCopyNonBlack = countNonBlack(bitmap)
            }

            var acceptedCapture = "PixelCopy"
            var acceptedPixelCount = pixelCopyNonBlack
            if (pixelCopyResult.get() != PixelCopy.SUCCESS || pixelCopyNonBlack <= 5000) {
                val rgba = nativeGetReadbackPixels(handle)
                if (rgba != null && rgba.size == surfaceWidth * surfaceHeight * 4) {
                    val fallback = bitmapFromGlRgba(rgba, surfaceWidth, surfaceHeight)
                    if (capturedBitmap !== fallback) capturedBitmap?.recycle()
                    capturedBitmap = fallback
                    val glPixels = countNonBlack(fallback)
                    if (glPixels > 5000) {
                        acceptedCapture = "glReadPixels"
                        acceptedPixelCount = glPixels
                    }
                }
            }

            val report = nativeGetVerificationReport(handle)
            val totalPixels = surfaceWidth * surfaceHeight
            val acceptedPixelPercent = if (totalPixels > 0) acceptedPixelCount * 100.0f / totalPixels else 0.0f
            val diagnostics = "capture=$acceptedCapture pixelCopyResult=${pixelCopyResult.get()} " +
                "pixelCopyNonBlack=$pixelCopyNonBlack acceptedNonBlack=$acceptedPixelCount " +
                "acceptedPercent=${acceptedPixelPercent}% bitmap=${surfaceWidth}x${surfaceHeight} totalPixels=$totalPixels " +
                "frameCount=${stats[0]} successfulPresentedFrames=${stats[12]} drawCalls=${stats[1]} triangles=${stats[2]} " +
                "vertices=${stats[3]} glReadbackNonBlack=${stats[7]} " +
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

    private fun countNonBlack(bitmap: Bitmap): Int {
        val pixels = IntArray(bitmap.width * bitmap.height)
        bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
        return pixels.count { color ->
            Color.red(color) > 10 || Color.green(color) > 10 || Color.blue(color) > 10
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
