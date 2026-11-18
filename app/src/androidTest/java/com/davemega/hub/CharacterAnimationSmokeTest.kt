package com.davemega.hub

import android.app.Activity
import android.app.Instrumentation
import android.content.Intent
import android.net.Uri
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
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CharacterAnimationSmokeTest {
    @Test
    fun namedButtonsPlayRealClipsAndRestoreUntouchedNodes() {
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
                device.wait(Until.hasObject(By.textContains("Playing: Walk")), 5_000))
            findVisible(device, By.text("Pause")).click()

            // The fixture's Walk clip moves node 'moving' from x=0 to x=1 over one second.
            // Read Filament's actual node transform, not just the Compose label.
            val viewer = findViewer(instrumentation, activity)
            val halfway = seekAndReadTranslation(instrumentation, viewer, 0.5f)
            assertEquals("Walk should move the animated node", 0.5f, halfway, 0.02f)

            scrollToTop(device)
            findVisible(device, By.desc("Wave animation")).click()
            assertTrue(device.wait(Until.hasObject(By.textContains("Playing: Wave")), 5_000))
            val reset = readTranslation(instrumentation, viewer)
            assertEquals("Switching clips must restore nodes not targeted by Wave", 0f, reset, 0.02f)
            assertFalse("A missing Run clip must not become a fake button",
                findVisible(device, By.desc("Run animation")).isEnabled)
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
            assertFalse(findVisible(device, By.desc("Walk animation")).isEnabled)
            assertFalse(findVisible(device, By.text("Play")).isEnabled)
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

    private fun readTranslation(instrumentation: Instrumentation, viewer: FilamentPreviewView): Float {
        var value = Float.NaN
        instrumentation.runOnMainSync { value = nodeTranslation(viewer) }
        return value
    }

    private fun nodeTranslation(viewer: FilamentPreviewView): Float {
        fun field(name: String): Any {
            val value = FilamentPreviewView::class.java.getDeclaredField(name)
            value.isAccessible = true
            return value.get(viewer) ?: throw AssertionError("Filament $name was unavailable")
        }
        val engine = field("engine") as Engine
        val asset = field("filamentAsset") as FilamentAsset
        val entity = asset.getFirstEntityByName("moving")
        assertTrue("Fixture node was not found", entity != 0)
        val transforms = engine.transformManager
        return transforms.getTransform(transforms.getInstance(entity), FloatArray(16))[12]
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
        val binary = ByteBuffer.allocate(184).order(ByteOrder.LITTLE_ENDIAN)
        floatArrayOf(-0.8f, -0.8f, 0f, 0.8f, -0.8f, 0f, 0f, 0.8f, 0f).forEach(binary::putFloat)
        repeat(3) { binary.putFloat(0f); binary.putFloat(0f); binary.putFloat(1f) }
        binary.putShort(0.toShort()); binary.putShort(1.toShort()); binary.putShort(2.toShort())
        binary.position(80)
        floatArrayOf(0f, 1f).forEach(binary::putFloat)
        // Idle, Wave, Walk, and an arbitrary imported animation.
        floatArrayOf(0f, 0f, 0f, 0f, 0f, 0f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 0f, 0.3f, 0f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 1f, 0f, 0f).forEach(binary::putFloat)
        floatArrayOf(0f, 0f, 0f, 0f, 0f, 0.2f).forEach(binary::putFloat)
        val json = """
            {
              "asset":{"version":"2.0"}, "scene":0,
              "scenes":[{"nodes":[0,1]}],
              "nodes":[{"name":"moving","mesh":0},{"name":"other"}],
              "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"indices":2,"material":0}]}],
              "materials":[{"doubleSided":true,"pbrMetallicRoughness":{"baseColorFactor":[0.8,0.3,0.2,1],"metallicFactor":0}}],
              "animations":[
                {"name":"idle","samplers":[{"input":3,"output":4}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
                {"name":"wave","samplers":[{"input":3,"output":5}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]},
                {"name":"walk","samplers":[{"input":3,"output":6}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
                {"name":"gesture_1","samplers":[{"input":3,"output":7}],"channels":[{"sampler":0,"target":{"node":1,"path":"translation"}}]}
              ],
              "accessors":[
                {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-0.8,-0.8,0],"max":[0.8,0.8,0]},
                {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
                {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
                {"bufferView":3,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},
                {"bufferView":4,"componentType":5126,"count":2,"type":"VEC3"},
                {"bufferView":5,"componentType":5126,"count":2,"type":"VEC3"},
                {"bufferView":6,"componentType":5126,"count":2,"type":"VEC3"},
                {"bufferView":7,"componentType":5126,"count":2,"type":"VEC3"}
              ],
              "bufferViews":[
                {"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},
                {"buffer":0,"byteOffset":36,"byteLength":36,"target":34962},
                {"buffer":0,"byteOffset":72,"byteLength":6,"target":34963},
                {"buffer":0,"byteOffset":80,"byteLength":8},
                {"buffer":0,"byteOffset":88,"byteLength":24},
                {"buffer":0,"byteOffset":112,"byteLength":24},
                {"buffer":0,"byteOffset":136,"byteLength":24},
                {"buffer":0,"byteOffset":160,"byteLength":24}
              ],
              "buffers":[{"byteLength":184}]
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
