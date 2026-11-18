package com.davemega.hub

import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.BySelector
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject2
import androidx.test.uiautomator.Until
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CharacterMenuSmokeTest {
    @Test
    fun homeOpensCharacterPresetsAndSelectionKeepsTheCurrentModel() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val device = UiDevice.getInstance(instrumentation)
        val prefs = context.getSharedPreferences("mega_hub", 0)
        val originalUri = prefs.getString("avatar_uri", null)
        val originalName = prefs.getString("avatar_name", null)
        prefs.edit().remove("avatar_uri").putString("avatar_name", "Existing model").commit()
        val launch = Intent(context, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        val activity = instrumentation.startActivitySync(launch)
        try {
            assertNotNull(activity)
            findVisible(device, By.textContains("Create Scout")).click()
            assertNotNull(device.wait(Until.findObject(By.desc("Scout preset")), 20_000))
            findVisible(device, By.text("Generate Scout"))
            scrollToTop(device)

            findVisible(device, By.desc("Sentinel preset")).click()
            findVisible(device, By.text("Generate Sentinel"))
            assertEquals("Existing model", prefs.getString("avatar_name", null))
            assertFalse("Selecting a preset must not generate or replace the current model", prefs.contains("avatar_uri"))

            findVisible(device, By.text("Home")).click()
            findVisible(device, By.text("Avatar")).click()
            findVisible(device, By.text("Generate Sentinel"))
            assertEquals("Existing model", prefs.getString("avatar_name", null))
        } finally {
            prefs.edit().apply {
                if (originalUri == null) remove("avatar_uri") else putString("avatar_uri", originalUri)
                if (originalName == null) remove("avatar_name") else putString("avatar_name", originalName)
            }.commit()
            activity?.finish()
        }
    }

    private fun scrollToTop(device: UiDevice) {
        repeat(3) {
            val width = device.displayWidth
            val height = device.displayHeight
            device.swipe(width / 2, (height * 0.32f).toInt(), width / 2, (height * 0.78f).toInt(), 18)
        }
    }

    private fun findVisible(device: UiDevice, selector: BySelector): UiObject2 {
        device.wait(Until.hasObject(selector), 3_000)
        repeat(5) {
            device.findObject(selector)?.let { return it }
            val width = device.displayWidth
            val height = device.displayHeight
            device.swipe(width / 2, (height * 0.78f).toInt(), width / 2, (height * 0.32f).toInt(), 18)
        }
        return device.findObject(selector) ?: throw AssertionError("Could not find $selector")
    }
}
