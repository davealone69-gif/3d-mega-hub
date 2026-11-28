package com.davemega.hub.avatarforge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RendererBackendPolicyTest {

    @Test
    fun parsesHardwareVulkanDescriptor() {
        val info = "vulkan|Samsung Xclipse 940|api 1.3"
        val parsed = RendererBackendPolicy.parse(info)
        assertEquals(RendererBackendPolicy.Backend.VULKAN, parsed.backend)
        assertEquals("api 1.3", parsed.apiVersion)
        assertEquals("Samsung Xclipse 940", parsed.deviceSummary)
        assertTrue(RendererBackendPolicy.isVulkan(info))
    }

    @Test
    fun parsesGlesFallbackDescriptor() {
        val info = "gles31|OpenGL ES 3.1 (SwiftShader)|Android Emulator OpenGL ES Translator (SwiftShader)"
        val parsed = RendererBackendPolicy.parse(info)
        assertEquals(RendererBackendPolicy.Backend.GLES31, parsed.backend)
        assertEquals("OpenGL ES 3.1 (SwiftShader)", parsed.apiVersion)
        assertEquals("Android Emulator OpenGL ES Translator (SwiftShader)", parsed.deviceSummary)
        assertFalse(RendererBackendPolicy.isVulkan(info))
    }

    @Test(expected = IllegalStateException::class)
    fun rejectsUnknownBackendPrefix() {
        RendererBackendPolicy.parse("metal|Apple M2|api 3.0")
    }
}
