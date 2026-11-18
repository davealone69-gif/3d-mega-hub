package com.davemega.hub

import android.app.Activity
import android.app.Instrumentation
import android.content.Intent
import android.graphics.Bitmap
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
import androidx.test.uiautomator.BySelector
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject2
import androidx.test.uiautomator.Until
import com.davemega.hub.avatarforge.GlbValidator
import com.google.android.filament.Engine
import com.google.android.filament.gltfio.FilamentAsset
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CharacterAnimationSmokeTest {
    @Test
    fun namedButtonsPlayClipsAndRestoreSameNodeAndMorphState() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val device = UiDevice.getInstance(instrumentation)
        val prefs = context.getSharedPreferences("mega_hub", 0)
        val originalUri = prefs.getString("avatar_uri", null)
        val originalName = prefs.getString("avatar_name", null)
        val directory = File(context.filesDir, "avatars")
        assertTrue(directory.isDirectory || directory.mkdirs())
        val fixture = File(directory, "animation-controls-smoke.glb")
        writeAnimatedFixture(fixture)
        GlbValidator.validate(fixture)
        prefs.edit().putString("avatar_uri", Uri.fromFile(fixture).toString())
            .putString("avatar_name", fixture.name).commit()

        val activity = instrumentation.startActivitySync(
            Intent(context, MainActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                .putExtra(EXTRA_OPEN_AVATAR_STUDIO, true)
        )
        try {
            assertNotNull(activity)
            findVisible(device, By.text("Animate your character"))
            findVisible(device, By.desc("Walk animation"))
            assertTrue("Animated GLB did not load a playable Walk clip",
                device.wait(Until.hasObject(By.desc("Walk animation").enabled(true)), 60_000))
            requireNotNull(device.findObject(By.desc("Walk animation"))) { "Walk button disappeared" }.click()
            assertTrue("Walk did not select its real glTF clip",
                device.wait(Until.hasObject(By.textContains("Playing: Walk")), 60_000))
            findVisible(device, By.text("Pause")).click()

            // The fixture's Walk clip moves node 'moving' from x=0 to x=1 over one second.
            // Read Filament's actual node transform, not just the Compose label.
            val viewer = findViewer(instrumentation, activity)
            val halfway = seekAndReadTranslation(instrumentation, viewer, 0.5f)
            assertEquals("Walk should move the animated node", 0.5f, halfway, 0.02f)
            assertEquals("Walk should move its second node", 0.25f,
                readTranslation(instrumentation, viewer, "omitted", 13), 0.02f)
            instrumentation.runOnMainSync {
                viewer.setAnimationLoop(false)
                viewer.setAnimationSpeed(1.5f)
            }

            scrollToTop(device)
            findVisible(device, By.desc("Wave animation")).click()
            assertTrue("Wave did not finish loading", device.wait(Until.hasObject(By.textContains("Playing: Wave")), 60_000))
            // Wave rotates the same node that Walk translates; the old internal translation
            // must not be recombined with Wave's rotation. It does not touch 'omitted'.
            assertEquals("Switching components on the same node must restore translation", 0f,
                readTranslation(instrumentation, viewer), 0.02f)
            assertEquals("Switching clips must restore an omitted node", 0f,
                readTranslation(instrumentation, viewer, "omitted", 13), 0.02f)
            assertFalse("A missing Run clip must not become a fake button",
                findVisible(device, By.desc("Run animation")).isEnabled)
            assertNotNull(findVisible(device, By.text("Loop: off")))

            // Compare actual Filament pixels: Expression modifies only the mesh's morph weight;
            // returning to Wave should restore the node's non-zero default morph weight.
            scrollToTop(device)
            pauseAndSeek(instrumentation, viewer, 0f)
            val baseline = captureSurface(instrumentation, viewer)
            selectAndWait(instrumentation, viewer, 4)
            pauseAndSeek(instrumentation, viewer, 1f)
            val morphed = captureSurface(instrumentation, viewer)
            selectAndWait(instrumentation, viewer, 1)
            pauseAndSeek(instrumentation, viewer, 0f)
            val restored = captureSurface(instrumentation, viewer)
            try {
                val changed = differingPixels(baseline, morphed)
                val remaining = differingPixels(baseline, restored)
                assertTrue("The morph-only clip did not visibly deform the fixture: $changed pixels", changed > 250)
                assertTrue("Morph weights were not restored when switching clips: changed=$changed remaining=$remaining",
                    remaining <= maxOf(100, changed / 10))
            } finally {
                baseline.recycle()
                morphed.recycle()
                restored.recycle()
            }
            findVisible(device, By.text("Stop / reset")).click()
            assertNotNull(findVisible(device, By.text("Play")))
        } finally {
            instrumentation.runOnMainSync { activity.finish() }
            prefs.edit().apply {
                if (originalUri == null) remove("avatar_uri") else putString("avatar_uri", originalUri)
                if (originalName == null) remove("avatar_name") else putString("avatar_name", originalName)
            }.commit()
            fixture.delete()
        }
    }

    @Test
    fun bundledStaticSampleShowsDisabledButtonsWithAnExplanation() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val device = UiDevice.getInstance(instrumentation)
        val prefs = context.getSharedPreferences("mega_hub", 0)
        val originalUri = prefs.getString("avatar_uri", null)
        val originalName = prefs.getString("avatar_name", null)
        prefs.edit().remove("avatar_uri").remove("avatar_name").commit()
        val activity = instrumentation.startActivitySync(
            Intent(context, MainActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                .putExtra(EXTRA_OPEN_AVATAR_STUDIO, true)
        )
        try {
            findVisible(device, By.text("Animate your character"))
            assertTrue("The static sample should explain why animation is unavailable",
                device.wait(Until.hasObject(By.textContains("This model has no animation clips")), 60_000))
            assertFalse("Static sample Walk shortcut should be disabled", findVisible(device, By.desc("Walk animation")).isEnabled)
            assertFalse("Static sample Play control should be disabled", findVisible(device, By.text("Play")).isEnabled)
        } finally {
            instrumentation.runOnMainSync { activity.finish() }
            prefs.edit().apply {
                if (originalUri == null) remove("avatar_uri") else putString("avatar_uri", originalUri)
                if (originalName == null) remove("avatar_name") else putString("avatar_name", originalName)
            }.commit()
        }
    }

    private fun findViewer(instrumentation: Instrumentation, activity: Activity): FilamentPreviewView {
        fun search(view: View): FilamentPreviewView? {
            if (view is FilamentPreviewView) return view
            if (view is ViewGroup) {
                for (i in 0 until view.childCount) search(view.getChildAt(i))?.let { return it }
            }
            return null
        }
        var result: FilamentPreviewView? = null
        instrumentation.runOnMainSync { result = search(activity.window.decorView) }
        return result ?: throw AssertionError("Filament preview was not attached")
    }

    private fun seekAndReadTranslation(instrumentation: Instrumentation, viewer: FilamentPreviewView, time: Float): Float {
        var value = Float.NaN
        instrumentation.runOnMainSync {
            viewer.seekAnimation(time)
            value = nodeTranslation(viewer)
        }
        return value
    }

    private fun readTranslation(
        instrumentation: Instrumentation,
        viewer: FilamentPreviewView,
        nodeName: String = "moving",
        component: Int = 12,
    ): Float {
        var value = Float.NaN
        instrumentation.runOnMainSync { value = nodeTranslation(viewer, nodeName, component) }
        return value
    }

    private fun nodeTranslation(viewer: FilamentPreviewView, nodeName: String = "moving", component: Int = 12): Float {
        fun field(name: String): Any {
            val value = FilamentPreviewView::class.java.getDeclaredField(name)
            value.isAccessible = true
            return value.get(viewer) ?: throw AssertionError("Filament $name was unavailable")
        }
        val engine = field("engine") as Engine
        val asset = field("filamentAsset") as FilamentAsset
        val entity = asset.getFirstEntityByName(nodeName)
        assertTrue("Fixture node '$nodeName' was not found", entity != 0)
        val transforms = engine.transformManager
        return transforms.getTransform(transforms.getInstance(entity), FloatArray(16))[component]
    }

    private fun selectAndWait(instrumentation: Instrumentation, viewer: FilamentPreviewView, index: Int) {
        instrumentation.runOnMainSync { viewer.selectAnimation(index) }
        val deadline = SystemClock.elapsedRealtime() + 60_000L
        while (SystemClock.elapsedRealtime() < deadline) {
            var ready = false
            instrumentation.runOnMainSync {
                val clips = FilamentPreviewView::class.java.getDeclaredField("animationClips").apply { isAccessible = true }
                val selected = FilamentPreviewView::class.java.getDeclaredField("selectedAnimation").apply { isAccessible = true }
                ready = clips.get(viewer) != null && selected.getInt(viewer) == index
            }
            if (ready) return
            SystemClock.sleep(100L)
        }
        throw AssertionError("Animation $index did not finish loading")
    }

    private fun pauseAndSeek(instrumentation: Instrumentation, viewer: FilamentPreviewView, position: Float) {
        instrumentation.runOnMainSync {
            viewer.setAnimationPlaying(false)
            viewer.seekAnimation(position)
        }
        // PixelCopy reads the next asynchronously rendered SurfaceView buffer.
        SystemClock.sleep(600L)
    }

    private fun captureSurface(instrumentation: Instrumentation, viewer: FilamentPreviewView): Bitmap {
        val surface = viewer.getChildAt(0) as SurfaceView
        assertTrue("Filament SurfaceView is not ready", surface.holder.surface.isValid && surface.width > 0 && surface.height > 0)
        val retryDelaysMs = longArrayOf(0L, 250L, 500L, 1_000L)
        for (delay in retryDelaysMs) {
            if (delay > 0L) SystemClock.sleep(delay)
            val image = Bitmap.createBitmap(surface.width, surface.height, Bitmap.Config.ARGB_8888)
            val done = CountDownLatch(1)
            var result = PixelCopy.ERROR_UNKNOWN
            instrumentation.runOnMainSync {
                PixelCopy.request(surface, image, { value -> result = value; done.countDown() }, Handler(Looper.getMainLooper()))
            }
            val completed = done.await(5, TimeUnit.SECONDS)
            if (completed && result == PixelCopy.SUCCESS) return image
            image.recycle()
            assertTrue("PixelCopy callback did not complete", completed)
            assertTrue("PixelCopy failed with result $result", result == PixelCopy.ERROR_TIMEOUT || result == PixelCopy.ERROR_SOURCE_NO_DATA)
        }
        throw AssertionError("Could not capture the Filament SurfaceView")
    }

    private fun differingPixels(first: Bitmap, second: Bitmap): Int {
        assertEquals(first.width, second.width)
        assertEquals(first.height, second.height)
        val firstPixels = IntArray(first.width * first.height)
        val secondPixels = IntArray(second.width * second.height)
        first.getPixels(firstPixels, 0, first.width, 0, 0, first.width, first.height)
        second.getPixels(secondPixels, 0, second.width, 0, 0, second.width, second.height)
        return firstPixels.indices.count { index ->
            val a = firstPixels[index]
            val b = secondPixels[index]
            val red = kotlin.math.abs(((a shr 16) and 255) - ((b shr 16) and 255))
            val green = kotlin.math.abs(((a shr 8) and 255) - ((b shr 8) and 255))
            val blue = kotlin.math.abs((a and 255) - (b and 255))
            red + green + blue > 24
        }
    }

    private fun scrollToTop(device: UiDevice) {
        repeat(4) {
            val width = device.displayWidth
            val height = device.displayHeight
            device.swipe(width / 2, (height * 0.30f).toInt(), width / 2, (height * 0.76f).toInt(), 18)
        }
    }

    private fun findVisible(device: UiDevice, selector: BySelector): UiObject2 {
        device.wait(Until.hasObject(selector), 3_000)
        repeat(6) {
            device.findObject(selector)?.let { return it }
            val width = device.displayWidth
            val height = device.displayHeight
            device.swipe(width / 2, (height * 0.76f).toInt(), width / 2, (height * 0.40f).toInt(), 18)
        }
        return device.findObject(selector) ?: throw AssertionError("Could not find $selector")
    }

    /** A tiny self-contained GLB with real node-animation channels in a non-shortcut order. */
    private fun writeAnimatedFixture(file: File) {
        val binary = ByteBuffer.allocate(284).order(ByteOrder.LITTLE_ENDIAN)
        floatArrayOf(-0.8f, -0.8f, 0f, 0.8f, -0.8f, 0f, 0f, 0.8f, 0f).forEach(binary::putFloat)
        repeat(3) { binary.putFloat(0f); binary.putFloat(0f); binary.putFloat(1f) }
        binary.putShort(0.toShort()); binary.putShort(1.toShort()); binary.putShort(2.toShort())
        binary.position(80)
        floatArrayOf(0f, 1f).forEach(binary::putFloat)
        // Idle, Wave, Walk and an arbitrary imported animation's translations.
        floatArrayOf(0f, 0f, 0f, 0f, 0f, 0f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 0f, 0.3f, 0f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 1f, 0f, 0f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 0f, 0f, 0.2f).forEach(binary::putFloat)
        // Wave rotates the same node Walk translates; Expression morphs its top vertex.
        floatArrayOf(0f, 0f, 0f, 1f, 0f, 0f, 0.70710677f, 0.70710677f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 0f, 0f, 0f, 0.8f, 0f, 0f).forEach(binary::putFloat)
        floatArrayOf(0.25f, 1f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 0f, 0.5f, 0f).forEach(binary::putFloat)
        val json = """
            {
              "asset":{"version":"2.0"}, "scene":0,
              "scenes":[{"nodes":[0,1,2]}],
              "nodes":[{"name":"moving","mesh":0,"weights":[0.25]},{"name":"other"},{"name":"omitted"}],
              "meshes":[{"weights":[0.1],"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"indices":2,"material":0,"targets":[{"POSITION":9}]}]}],
              "materials":[{"doubleSided":true,"pbrMetallicRoughness":{"baseColorFactor":[0.8,0.3,0.2,1],"metallicFactor":0}}],
              "animations":[
                {"name":"idle","samplers":[{"input":3,"output":4}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
                {"name":"wave","samplers":[{"input":3,"output":5},{"input":3,"output":8}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}},{"sampler":1,"target":{"node":0,"path":"rotation"}}]},
                {"name":"walk","samplers":[{"input":3,"output":6},{"input":3,"output":11}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}},{"sampler":1,"target":{"node":2,"path":"translation"}}]},
                {"name":"gesture_1","samplers":[{"input":3,"output":7}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]},
                {"name":"expression","samplers":[{"input":3,"output":10}],"channels":[{"sampler":0,"target":{"node":0,"path":"weights"}}]}
              ],
              "accessors":[
                {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-0.8,-0.8,0],"max":[0.8,0.8,0]},
                {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
                {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
                {"bufferView":3,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
                {"bufferView":4,"componentType":5126,"count":2,"type":"VEC3"},
                {"bufferView":5,"componentType":5126,"count":2,"type":"VEC3"},
                {"bufferView":6,"componentType":5126,"count":2,"type":"VEC3"},
                {"bufferView":7,"componentType":5126,"count":2,"type":"VEC3"},
                {"bufferView":8,"componentType":5126,"count":2,"type":"VEC4"},
                {"bufferView":9,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[0.8,0,0]},
                {"bufferView":10,"componentType":5126,"count":2,"type":"SCALAR"},
                {"bufferView":11,"componentType":5126,"count":2,"type":"VEC3"}
              ],
              "bufferViews":[
                {"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},
                {"buffer":0,"byteOffset":36,"byteLength":36,"target":34962},
                {"buffer":0,"byteOffset":72,"byteLength":6,"target":34963},
                {"buffer":0,"byteOffset":80,"byteLength":8},
                {"buffer":0,"byteOffset":88,"byteLength":24},
                {"buffer":0,"byteOffset":112,"byteLength":24},
                {"buffer":0,"byteOffset":136,"byteLength":24},
                {"buffer":0,"byteOffset":160,"byteLength":24},
                {"buffer":0,"byteOffset":184,"byteLength":32},
                {"buffer":0,"byteOffset":216,"byteLength":36,"target":34962},
                {"buffer":0,"byteOffset":252,"byteLength":8},
                {"buffer":0,"byteOffset":260,"byteLength":24}
              ],
              "buffers":[{"byteLength":284}]
            }
        """.trimIndent().toByteArray(Charsets.UTF_8)
        val paddedJsonSize = (json.size + 3) and -4
        val totalSize = 12 + 8 + paddedJsonSize + 8 + binary.capacity()
        val glb = ByteBuffer.allocate(totalSize).order(ByteOrder.LITTLE_ENDIAN)
        glb.putInt(0x46546C67); glb.putInt(2); glb.putInt(totalSize)
        glb.putInt(paddedJsonSize); glb.putInt(0x4E4F534A); glb.put(json)
        repeat(paddedJsonSize - json.size) { glb.put(0x20) }
        glb.putInt(binary.capacity()); glb.putInt(0x004E4942); glb.put(binary.array())
        file.writeBytes(glb.array())
    }
}
