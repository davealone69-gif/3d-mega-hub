package com.davemega.hub

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Rect
import android.net.Uri
import android.os.SystemClock
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import com.davemega.hub.avatarforge.AvatarForgeSurfaceView
import java.io.File
import java.io.FileOutputStream
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AvatarForgeRendererSmokeTest {
    @Test
    fun importedBundledGlbUsesAvatarForgeGlesAndProducesVisibleGeometry() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val device = UiDevice.getInstance(instrumentation)

        val avatarDirectory = File(context.filesDir, "avatars")
        assertTrue("Could not create app-private avatar test directory", avatarDirectory.isDirectory || avatarDirectory.mkdirs())
        val glb = File(avatarDirectory, "avatarforge-renderer-smoke.glb")
        context.assets.open("models/female.glb").use { source ->
            FileOutputStream(glb).use { target -> source.copyTo(target) }
        }
        assertTrue("Bundled smoke GLB is empty", glb.length() > 0L)

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

        assertTrue(
            "AvatarForge SurfaceView was never attached. The test must exercise the imported-GLB route, not Filament's bundled-sample viewer.",
            device.wait(Until.hasObject(By.clazz(AvatarForgeSurfaceView::class.java.name)), 20_000)
        )

        val prefs = context.getSharedPreferences("avatarforge_renderer_smoke", 0)
        val deadline = SystemClock.elapsedRealtime() + 90_000L
        while (SystemClock.elapsedRealtime() < deadline) {
            if (prefs.getString("status", "") == "FRAME_VERIFIED" ||
                prefs.getString("status", "") == "FAILED") break
            SystemClock.sleep(500L)
        }

        val status = prefs.getString("status", "")
        assertTrue(
            "AvatarForge native GLES renderer did not verify a real frame. status=$status error=${prefs.getString("error", "")} api=${prefs.getString("api_version", "")}",
            status == "FRAME_VERIFIED"
        )
        val apiVersion = prefs.getString("api_version", "") ?: ""
        assertTrue("Renderer did not report OpenGL ES 3.1 or newer: $apiVersion", apiVersion.matches(Regex("OpenGL ES 3\\.[1-9].*")))
        assertTrue("Renderer reported no submitted frames", prefs.getInt("frames", 0) >= 3)
        assertTrue("Renderer reported no draw calls", prefs.getInt("draw_calls", 0) > 0)
        assertTrue("Renderer reported no model triangles", prefs.getInt("triangles", 0) > 0)

        val viewport = device.findObject(By.clazz(AvatarForgeSurfaceView::class.java.name))
        assertNotNull("AvatarForge viewport disappeared after rendering", viewport)
        val bounds = viewport!!.visibleBounds
        assertTrue("AvatarForge viewport is not visible: $bounds", bounds.width() > 100 && bounds.height() > 100)

        val screenshot = device.takeScreenshot()
        assertNotNull("Could not capture emulator screen", screenshot)
        val image = screenshot!!
        val crop = Rect(bounds).apply {
            intersect(0, 0, image.width, image.height)
        }
        assertTrue("AvatarForge viewport lies outside the captured screen: $crop", crop.width() > 0 && crop.height() > 0)

        // Persist evidence before assertions so a failing run still carries the exact viewport.
        val externalDirectory = requireNotNull(context.getExternalFilesDir(null)) {
            "App external files directory is unavailable"
        }
        val output = File(externalDirectory, "avatarforge-renderer-smoke.png")
        FileOutputStream(output).use { stream ->
            assertTrue("Could not encode captured AvatarForge viewport", image.compress(Bitmap.CompressFormat.PNG, 100, stream))
        }
        assertTrue("Captured renderer screenshot is missing", output.isFile && output.length() > 0L)

        // Derive a background reference from the four viewport corners. Then demand a spatially
        // substantial region of pixels that differs from that background, not just shader dither
        // or a non-uniform clear pass.
        val cornerSamples = ArrayList<Int>()
        val cornerSizeX = (crop.width() / 12).coerceAtLeast(1)
        val cornerSizeY = (crop.height() / 12).coerceAtLeast(1)
        for (cornerY in listOf(crop.top, crop.bottom - cornerSizeY)) {
            for (cornerX in listOf(crop.left, crop.right - cornerSizeX)) {
                var y = cornerY
                while (y < cornerY + cornerSizeY) {
                    var x = cornerX
                    while (x < cornerX + cornerSizeX) {
                        cornerSamples.add(image.getPixel(x, y))
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
                val pixel = image.getPixel(x, y)
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
