package com.davemega.hub.avatarforge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The frame budget must stay strict on real hardware and only widen for software rasterisers.
 * Emulator GL strings observed in the exact-head CI evidence drive these cases.
 */
class RendererTimeBudgetTest {
    private val hardwareGl = "OpenGL ES 3.2 Driver version: 25.6.4 git hash: 9fe6623ec0db"
    private val hardwareFingerprint = "samsung/e3qxxx/e3q:14/UP1A.231005.007/S921BXXU1AXBA:user/release-keys"

    @Test
    fun hardwareRendererKeepsTheStrictBudget() {
        assertEquals(
            RendererTimeBudget.HARDWARE_FRAME_WAIT_MS,
            RendererTimeBudget.frameWaitMillis(hardwareGl, hardwareFingerprint, "SM-S921B")
        )
        assertFalse(RendererTimeBudget.isSoftwareRenderer(hardwareGl, hardwareFingerprint, "SM-S921B"))
    }

    @Test
    fun swiftshaderGetsTheSoftwareBudget() {
        val apiVersion = "OpenGL ES 3.0 SwiftShader"
        assertEquals(
            RendererTimeBudget.SOFTWARE_FRAME_WAIT_MS,
            RendererTimeBudget.frameWaitMillis(apiVersion, "google/sdk_gphone64_x86_64/emu64xa:14", "sdk_gphone64_x86_64")
        )
    }

    @Test
    fun emulatorFingerprintAloneWidensTheBudget() {
        assertEquals(
            RendererTimeBudget.SOFTWARE_FRAME_WAIT_MS,
            RendererTimeBudget.frameWaitMillis(
                "OpenGL ES 3.1",
                "google/sdk_gphone64_x86_64/emu64xa:14/UE1A.230829.036/11228840:userdebug/dev-keys"
            )
        )
    }

    @Test
    fun softwareBudgetIsAlwaysTheLargerOne() {
        assertTrue(RendererTimeBudget.SOFTWARE_FRAME_WAIT_MS > RendererTimeBudget.HARDWARE_FRAME_WAIT_MS)
    }
}
