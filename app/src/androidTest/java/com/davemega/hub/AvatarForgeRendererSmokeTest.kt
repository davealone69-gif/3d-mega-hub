package com.davemega.hub

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Rect
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.PixelCopy
import android.view.SurfaceView
import android.view.View
import android.view.ViewGroup
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AvatarForgeRendererSmokeTest {
    @Test
    fun importedBundledGlbUsesAvatarForgeGlesAndProducesVisibleGeometry() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val directory = File(context.filesDir, "avatars")
        assertTrue("Could not create app-private avatar test directory", directory.isDirectory || directory.mkdirs())
        val glb = File(directory, "avatarforge-renderer-smoke.glb")
        context.assets.open("models/female.glb").use { source ->
            FileOutputStream(glb).use { target -> source.copyTo(target) }
        }
        assertTrue("Bundled smoke GLB is empty", glb.length() > 0L)
        verifyImportedGlbThroughTheApp(glb)
    }

    @Test
    fun requiredWebpTextureExtensionIsDecodedAndRenderedByAvatarForge() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val directory = File(context.filesDir, "avatars")
        assertTrue("Could not create app-private avatar test directory", directory.isDirectory || directory.mkdirs())
        val glb = File(directory, "avatarforge-webp-extension-smoke.glb")
        writeRequiredWebpFixture(glb)
        assertTrue("Generated WebP GLB fixture is empty", glb.length() > 0L)
        verifyImportedGlbThroughTheApp(glb)
    }

    @Test
    fun generatedAvatarDebugDiagnosticVerifiesImportedGlesPixels() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val device = UiDevice.getInstance(instrumentation)
        context.getSharedPreferences("mega_hub", 0).edit()
            .remove("avatar_uri")
            .remove("avatar_name")
            .commit()
        val prefs = context.getSharedPreferences("avatarforge_renderer_smoke", 0)
        prefs.edit().clear().commit()

        val launch = Intent(context, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            .putExtra(EXTRA_OPEN_AVATAR_STUDIO, true)
        val activity = instrumentation.startActivitySync(launch)
        assertNotNull("MainActivity did not launch for the generated-avatar diagnostic", activity)

        val button = By.text("Run generated-avatar GLES diagnostic")
        assertTrue("Debug-only generated-avatar diagnostic button is missing",
            device.wait(Until.hasObject(button), 20_000))
        val diagnosticButton = device.findObject(button)
        assertNotNull("Generated-avatar diagnostic button disappeared", diagnosticButton)
        diagnosticButton!!.click()

        val deadline = SystemClock.elapsedRealtime() + 300_000L
        while (SystemClock.elapsedRealtime() < deadline) {
            val status = prefs.getString("status", "")
            if (status == "PIXELS_VERIFIED" || status == "FAILED") break
            SystemClock.sleep(500L)
        }

        val status = prefs.getString("status", "")
        assertTrue(
            "Generated-avatar in-app GLES diagnostic did not verify visible geometry. status=$status error=${prefs.getString("error", "")} generation=${prefs.getString("generation_report", "")}",
            status == "PIXELS_VERIFIED"
        )
        val report = prefs.getString("generation_report", "") ?: ""
        assertTrue("Seed-1337 native generation report is missing or validation failed: $report",
            report.contains("seed=1337") && report.contains("deformed_mesh_validated=true"))
        fun generatedMetric(name: String): Long =
            Regex("""\b${Regex.escape(name)}=(\d+)\b""")
                .find(report)?.groupValues?.getOrNull(1)?.toLongOrNull() ?: 0L
        assertTrue("Generated avatar did not report live vertex count: $report", generatedMetric("vertices") > 0L)
        assertTrue("Generated avatar did not report live triangle count: $report", generatedMetric("triangles") > 0L)
        assertTrue("Generated avatar did not report live joint count: $report", generatedMetric("joints") > 0L)
        assertTrue("Generated avatar did not report live blendshape count: $report", generatedMetric("blendshapes") > 0L)
        assertTrue("Generated GLB output is empty: $report", generatedMetric("bytes") > 0L)
        val apiVersion = prefs.getString("api_version", "") ?: ""
        assertTrue("Generated avatar did not render through OpenGL ES 3.1+: $apiVersion",
            apiVersion.matches(Regex("OpenGL ES 3\\.[1-9].*")))
        assertTrue("Generated avatar had fewer than three successful EGL swaps",
            prefs.getInt("presented_frames", 0) >= 3)
        assertTrue("Generated avatar produced no triangles", prefs.getInt("triangles", 0) > 0)
        assertTrue("Generated avatar bounds were outside the frustum",
            prefs.getBoolean("bounds_in_frustum", false))
        assertTrue("Generated avatar reported a GLES error", prefs.getInt("gl_error", -1) == 0)
        assertTrue("Generated avatar reported an EGL error", prefs.getInt("egl_error", -1) == 0)
        assertTrue("Generated avatar did not produce more than 5,000 geometry pixels",
            prefs.getInt("pixel_count", 0) > 5_000)

        val screenshotPath = prefs.getString("screenshot_path", "") ?: ""
        val screenshot = File(screenshotPath)
        assertTrue("Generated-avatar diagnostic screenshot is missing: $screenshotPath",
            screenshot.isFile && screenshot.length() > 0L)
    }

    private fun writeRequiredWebpFixture(file: File) {
        val bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888)
        val webpBytes = try {
            bitmap.eraseColor(Color.MAGENTA)
            ByteArrayOutputStream().use { output ->
                assertTrue(
                    "Android platform could not encode the WebP fixture",
                    bitmap.compress(Bitmap.CompressFormat.WEBP_LOSSY, 100, output)
                )
                output.toByteArray()
            }
        } finally {
            bitmap.recycle()
        }
        assertTrue("WebP fixture encoder returned invalid bytes", webpBytes.size >= 12 &&
            webpBytes[0] == 'R'.code.toByte() && webpBytes[8] == 'W'.code.toByte())

        // POSITION(36) + NORMAL(36) + TEXCOORD_0(24) + indices(6) + 2-byte alignment.
        val imageOffset = 104
        val binarySize = (imageOffset + webpBytes.size + 3) and -4
        val binary = ByteBuffer.allocate(binarySize).order(ByteOrder.LITTLE_ENDIAN)
        floatArrayOf(
            -0.82f, -0.82f, 0.0f,
             0.82f, -0.82f, 0.0f,
             0.00f,  0.82f, 0.0f
        ).forEach(binary::putFloat)
        repeat(3) {
            binary.putFloat(0.0f)
            binary.putFloat(0.0f)
            binary.putFloat(1.0f)
        }
        floatArrayOf(0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f).forEach { binary.putFloat(it) }
        // Counter-clockwise front face for the viewer at +Z looking toward -Z.
        binary.putShort(0.toShort())
        binary.putShort(1.toShort())
        binary.putShort(2.toShort())
        binary.position(imageOffset)
        binary.put(webpBytes)

        val json = """
            {
              "asset":{"version":"2.0","generator":"AvatarForge Android WebP smoke fixture"},
              "extensionsUsed":["EXT_texture_webp"],
              "extensionsRequired":["EXT_texture_webp"],
              "scene":0,
              "scenes":[{"nodes":[0]}],
              "nodes":[{"mesh":0}],
              "meshes":[{"primitives":[{
                "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
                "indices":3,
                "material":0
              }]}],
              "materials":[{"pbrMetallicRoughness":{
                "baseColorFactor":[1,1,1,1],
                "metallicFactor":0,
                "roughnessFactor":1,
                "baseColorTexture":{"index":0}
              }}],
              "textures":[{"extensions":{"EXT_texture_webp":{"source":0}}}],
              "images":[{"mimeType":"image/webp","bufferView":4}],
              "accessors":[
                {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-0.82,-0.82,0],"max":[0.82,0.82,0]},
                {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
                {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
                {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}
              ],
              "bufferViews":[
                {"buffer":0,"byteOffset":0,"byteLength":36},
                {"buffer":0,"byteOffset":36,"byteLength":36},
                {"buffer":0,"byteOffset":72,"byteLength":24},
                {"buffer":0,"byteOffset":96,"byteLength":6},
                {"buffer":0,"byteOffset":104,"byteLength":${webpBytes.size}}
              ],
              "buffers":[{"byteLength":$binarySize}]
            }
        """.trimIndent().toByteArray(Charsets.UTF_8)
        val paddedJsonSize = (json.size + 3) and -4
        val totalSize = 12 + 8 + paddedJsonSize + 8 + binarySize
        val glb = ByteBuffer.allocate(totalSize).order(ByteOrder.LITTLE_ENDIAN)
        glb.putInt(0x46546C67)
        glb.putInt(2)
        glb.putInt(totalSize)
        glb.putInt(paddedJsonSize)
        glb.putInt(0x4E4F534A)
        glb.put(json)
        repeat(paddedJsonSize - json.size) { glb.put(0x20) }
        glb.putInt(binarySize)
        glb.putInt(0x004E4942)
        glb.put(binary.array())
        file.writeBytes(glb.array())
    }

    private fun verifyImportedGlbThroughTheApp(glb: File) {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val device = UiDevice.getInstance(instrumentation)
        assertTrue("GLB fixture is not readable", glb.isFile && glb.canRead() && glb.length() > 0L)

        context.getSharedPreferences("mega_hub", 0).edit()
            .putString("avatar_uri", Uri.fromFile(glb).toString())
            .putString("avatar_name", glb.name)
            .commit()
        context.getSharedPreferences("avatarforge_renderer_smoke", 0).edit().clear().commit()

        val launch = Intent(context, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            .putExtra(EXTRA_OPEN_AVATAR_STUDIO, true)
        val activity = instrumentation.startActivitySync(launch)
        assertNotNull("MainActivity did not launch", activity)

        val prefs = context.getSharedPreferences("avatarforge_renderer_smoke", 0)
        fun findAvatarForgeSurface(view: View): View? {
            if (view.javaClass.name == "com.davemega.hub.avatarforge.AvatarForgeSurfaceView") return view
            if (view is ViewGroup) {
                for (index in 0 until view.childCount) {
                    findAvatarForgeSurface(view.getChildAt(index))?.let { return it }
                }
            }
            return null
        }

        // The viewport is below the import and generation controls in a vertically scrolling
        // Compose screen. Bring it on-screen so SurfaceView receives surfaceCreated; merely
        // launching the Avatar tab does not guarantee that an off-screen SurfaceView has a surface.
        val scrollDeadline = SystemClock.elapsedRealtime() + 30_000L
        var viewport = findAvatarForgeSurface(activity.window.decorView)
        while ((viewport == null || !viewport.isShown || !viewport.isAttachedToWindow) &&
            SystemClock.elapsedRealtime() < scrollDeadline) {
            val width = device.displayWidth
            val height = device.displayHeight
            device.swipe(width / 2, (height * 0.82f).toInt(), width / 2, (height * 0.22f).toInt(), 18)
            SystemClock.sleep(400L)
            viewport = findAvatarForgeSurface(activity.window.decorView)
        }
        assertNotNull(
            "Imported GLB did not create an AvatarForgeSurfaceView after scrolling the Avatar Studio viewport into view.",
            viewport
        )
        val avatarForgeView = requireNotNull(viewport) { "Imported AvatarForge viewport was not created." }
        assertTrue("AvatarForge SurfaceView is not attached to the Activity window",
            avatarForgeView.isAttachedToWindow && avatarForgeView.isShown &&
                avatarForgeView.width > 100 && avatarForgeView.height > 100)

        val deadline = SystemClock.elapsedRealtime() + 90_000L
        while (SystemClock.elapsedRealtime() < deadline) {
            if (prefs.getString("status", "") == "FRAME_PRESENTED" ||
                prefs.getString("status", "") == "FAILED") break
            SystemClock.sleep(500L)
        }

        val status = prefs.getString("status", "")
        assertTrue(
            "AvatarForge native GLES renderer did not verify a real frame for ${glb.name}. status=$status error=${prefs.getString("error", "")} api=${prefs.getString("api_version", "")}",
            status == "FRAME_PRESENTED"
        )
        val apiVersion = prefs.getString("api_version", "") ?: ""
        assertTrue("Renderer did not report OpenGL ES 3.1 or newer: $apiVersion", apiVersion.matches(Regex("OpenGL ES 3\\.[1-9].*")))
        assertTrue("Renderer reported fewer than three successful EGL swaps", prefs.getInt("presented_frames", 0) >= 3)
        assertTrue("Renderer reported no draw calls", prefs.getInt("draw_calls", 0) > 0)
        assertTrue("Renderer reported no model triangles", prefs.getInt("triangles", 0) > 0)
        assertTrue("Model bounds were outside the camera frustum", prefs.getBoolean("bounds_in_frustum", false))
        assertTrue("Renderer reported a GLES error", prefs.getInt("gl_error", -1) == 0)
        assertTrue("Renderer reported an EGL presentation error", prefs.getInt("egl_error", -1) == 0)

        /* Capture the SurfaceView buffer itself. UiAutomation.takeScreenshot() can omit or
           capture a separately-composited SurfaceView as its background on emulator/GPU paths;
           it is not valid evidence for a native GLES texture sample. */
        val surfaceView = avatarForgeView as? SurfaceView
            ?: throw AssertionError("AvatarForgeSurfaceView is not backed by Android SurfaceView")
        // SurfaceView presentation and PixelCopy readback are asynchronous. A swapped
        // frame does not guarantee the compositor has exposed its buffer yet, particularly
        // on software-rendered CI emulators. Retry only documented transient readback errors.
        val surfaceReadyDeadline = SystemClock.elapsedRealtime() + 10_000L
        while ((!surfaceView.holder.surface.isValid || !surfaceView.isShown ||
                !surfaceView.isAttachedToWindow) &&
            SystemClock.elapsedRealtime() < surfaceReadyDeadline) {
            SystemClock.sleep(100L)
        }
        assertTrue("AvatarForge SurfaceView surface never became valid for PixelCopy",
            surfaceView.holder.surface.isValid && surfaceView.isShown && surfaceView.isAttachedToWindow)
        SystemClock.sleep(500L)

        var image: Bitmap? = null
        var pixelCopyResult = PixelCopy.ERROR_UNKNOWN
        val retryDelaysMs = longArrayOf(0L, 250L, 500L, 1_000L)
        for ((attempt, delayMs) in retryDelaysMs.withIndex()) {
            if (delayMs > 0L) SystemClock.sleep(delayMs)
            val candidate = Bitmap.createBitmap(surfaceView.width, surfaceView.height, Bitmap.Config.ARGB_8888)
            val pixelCopyLatch = CountDownLatch(1)
            var attemptResult = PixelCopy.ERROR_UNKNOWN
            instrumentation.runOnMainSync {
                PixelCopy.request(surfaceView, candidate, { result ->
                    attemptResult = result
                    pixelCopyLatch.countDown()
                }, Handler(Looper.getMainLooper()))
            }
            val callbackArrived = pixelCopyLatch.await(5, TimeUnit.SECONDS)
            if (callbackArrived) pixelCopyResult = attemptResult
            if (callbackArrived && attemptResult == PixelCopy.SUCCESS) {
                image = candidate
                break
            }
            candidate.recycle()
            assertTrue(
                "PixelCopy callback did not complete on attempt ${attempt + 1}/${retryDelaysMs.size}; surfaceValid=${surfaceView.holder.surface.isValid} size=${surfaceView.width}x${surfaceView.height}",
                callbackArrived
            )
            assertTrue("PixelCopy failed with non-retryable result=$attemptResult on attempt ${attempt + 1}",
                attemptResult == PixelCopy.ERROR_TIMEOUT || attemptResult == PixelCopy.ERROR_SOURCE_NO_DATA)
        }
        assertTrue(
            "PixelCopy could not capture the actual AvatarForge SurfaceView after ${retryDelaysMs.size} attempts; lastResult=$pixelCopyResult",
            image != null
        )
        val capturedImage = requireNotNull(image)
        val crop = Rect(0, 0, capturedImage.width, capturedImage.height)
        assertTrue("AvatarForge SurfaceView capture has invalid dimensions: $crop",
            crop.width() > 0 && crop.height() > 0)

        // Persist the exact renderer buffer before colour/coverage assertions so failures retain evidence.
        val externalDirectory = requireNotNull(context.getExternalFilesDir(null)) {
            "App external files directory is unavailable"
        }
        val output = File(externalDirectory, "avatarforge-renderer-smoke.png")
        FileOutputStream(output).use { stream ->
            assertTrue("Could not encode captured AvatarForge SurfaceView",
                capturedImage.compress(Bitmap.CompressFormat.PNG, 100, stream))
        }
        assertTrue("Captured renderer screenshot is missing", output.isFile && output.length() > 0L)

        if (glb.name.contains("webp-extension")) {
            val texturedCenter = capturedImage.getPixel(crop.centerX(), crop.centerY())
            val red = (texturedCenter shr 16) and 0xff
            val green = (texturedCenter shr 8) and 0xff
            val blue = texturedCenter and 0xff
            assertTrue(
                "Required WebP texture was not visible in PixelCopy SurfaceView buffer at the triangle centre: RGB=($red,$green,$blue)",
                red > green + 20 && blue > green + 20 && red > 40 && blue > 40
            )
        }

        // Compare against a robust four-corner background estimate. A successful frame counter or
        // post-process draw alone is not accepted as evidence of visible model geometry.
        val cornerSamples = ArrayList<Int>()
        val cornerSizeX = (crop.width() / 12).coerceAtLeast(1)
        val cornerSizeY = (crop.height() / 12).coerceAtLeast(1)
        for (cornerY in listOf(crop.top, crop.bottom - cornerSizeY)) {
            for (cornerX in listOf(crop.left, crop.right - cornerSizeX)) {
                var y = cornerY
                while (y < cornerY + cornerSizeY) {
                    var x = cornerX
                    while (x < cornerX + cornerSizeX) {
                        cornerSamples.add(capturedImage.getPixel(x, y))
                        x += (cornerSizeX / 8).coerceAtLeast(1)
                    }
                    y += (cornerSizeY / 8).coerceAtLeast(1)
                }
            }
        }
        fun medianChannel(shift: Int): Int =
            cornerSamples.map { (it shr shift) and 0xff }.sorted()[cornerSamples.size / 2]
        val backgroundR = medianChannel(16)
        val backgroundG = medianChannel(8)
        val backgroundB = medianChannel(0)

        val gridWidth = 96
        val gridHeight = 96
        var changedSamples = 0
        var centralChangedSamples = 0
        var minChangedX = gridWidth
        var maxChangedX = -1
        var minChangedY = gridHeight
        var maxChangedY = -1
        for (gy in 0 until gridHeight) {
            val y = crop.top + ((gy + 0.5f) * crop.height() / gridHeight).toInt()
            for (gx in 0 until gridWidth) {
                val x = crop.left + ((gx + 0.5f) * crop.width() / gridWidth).toInt()
                val pixel = capturedImage.getPixel(x, y)
                val red = (pixel shr 16) and 0xff
                val green = (pixel shr 8) and 0xff
                val blue = pixel and 0xff
                val difference = maxOf(
                    kotlin.math.abs(red - backgroundR),
                    kotlin.math.abs(green - backgroundG),
                    kotlin.math.abs(blue - backgroundB)
                )
                if (difference > 32) {
                    changedSamples++
                    minChangedX = minOf(minChangedX, gx)
                    maxChangedX = maxOf(maxChangedX, gx)
                    minChangedY = minOf(minChangedY, gy)
                    maxChangedY = maxOf(maxChangedY, gy)
                    if (gx in 12..83 && gy in 8..87) centralChangedSamples++
                }
            }
        }
        val coverage = changedSamples.toFloat() / (gridWidth * gridHeight)
        assertTrue(
            "Viewport pixels do not contain substantial rendered content: changed=${changedSamples}/${gridWidth * gridHeight} (${coverage * 100}%), central=${centralChangedSamples}, bounds=[${minChangedX},${minChangedY}..${maxChangedX},${maxChangedY}], background=(${backgroundR},${backgroundG},${backgroundB}). Screenshot saved at $output",
            coverage >= 0.02f && centralChangedSamples >= 80 &&
                maxChangedX - minChangedX >= 14 && maxChangedY - minChangedY >= 14
        )
    }
}
