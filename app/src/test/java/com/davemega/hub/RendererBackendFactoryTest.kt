package com.davemega.hub

import com.google.android.filament.Engine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RendererBackendFactoryTest {
    @Test
    fun vulkanIsPrimaryAndStopsAfterSuccessfulInitialization() {
        val attempts = mutableListOf<Engine.Backend>()

        val selection = createWithRendererBackendFallback { backend ->
            attempts += backend
            "${backend.displayName()} session"
        }

        assertEquals(Engine.Backend.VULKAN, selection.backend)
        assertEquals("Vulkan session", selection.value)
        assertEquals(listOf(Engine.Backend.VULKAN), attempts)
    }

    @Test
    fun fallsBackToOpenGlEsWhenVulkanInitializationThrows() {
        val attempts = mutableListOf<Engine.Backend>()

        val selection = createWithRendererBackendFallback { backend ->
            attempts += backend
            if (backend == Engine.Backend.VULKAN) throw UnsupportedOperationException("Vulkan unavailable")
            "GLES session"
        }

        assertEquals(Engine.Backend.OPENGL, selection.backend)
        assertEquals("GLES session", selection.value)
        assertEquals(listOf(Engine.Backend.VULKAN, Engine.Backend.OPENGL), attempts)
    }

    @Test
    fun fallsBackToOpenGlEsWhenVulkanInitializationReturnsNull() {
        val attempts = mutableListOf<Engine.Backend>()

        val selection = createWithRendererBackendFallback { backend ->
            attempts += backend
            if (backend == Engine.Backend.VULKAN) null else "GLES session"
        }

        assertEquals(Engine.Backend.OPENGL, selection.backend)
        assertEquals("GLES session", selection.value)
        assertEquals(listOf(Engine.Backend.VULKAN, Engine.Backend.OPENGL), attempts)
    }

    @Test
    fun reportsFailuresWhenNeitherBackendInitializes() {
        try {
            createWithRendererBackendFallback<String> { backend ->
                throw IllegalStateException("${backend.displayName()} failed")
            }
            throw AssertionError("Expected both backend attempts to fail")
        } catch (failure: IllegalStateException) {
            assertTrue(failure.message!!.contains("Vulkan"))
            assertEquals(2, failure.suppressed.size)
        }
    }
}
