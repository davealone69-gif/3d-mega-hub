package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Test

class RuntimeCapabilityTest {
    @Test
    fun missingBackendIsNotIntegrated() {
        assertEquals("NOT INTEGRATED", RuntimeCapability("Renderer").status)
    }

    @Test
    fun configuredButUninitializedBackendIsNotVerified() {
        assertEquals(
            "UNVERIFIED",
            RuntimeCapability("Renderer", backend = "Filament/OpenGL ES").status
        )
    }

    @Test
    fun initializedBackendIsNotMislabelledDeviceVerified() {
        assertEquals(
            "INITIALIZED, NOT DEVICE-VERIFIED",
            RuntimeCapability("Renderer", backend = "Filament/OpenGL ES", initialized = true).status
        )
    }
}
