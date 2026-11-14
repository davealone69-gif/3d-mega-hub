package com.davemega.hub

import com.google.android.filament.Engine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RendererBackendFactoryTest {
    @Test
    fun openGlEsIsPrimaryAndStopsAfterSuccessfulInitialization() {
        val attempts = mutableListOf<Engine.Backend>()
        val selection = createWithRendererBackendFallback { backend ->
            attempts += backend
            "${backend.displayName()} session"
        }
        assertEquals(Engine.Backend.OPENGL, selection.backend)
        assertEquals("OpenGL ES session", selection.value)
        assertEquals(listOf(Engine.Backend.OPENGL), attempts)
    }

    @Test
    fun fallsBackToVulkanWhenOpenGlEsInitializationThrows() {
        val attempts = mutableListOf<Engine.Backend>()
        val selection = createWithRendererBackendFallback { backend ->
            attempts += backend
            if (backend == Engine.Backend.OPENGL) throw UnsupportedOperationException("GLES unavailable")
            "Vulkan session"
        }
        assertEquals(Engine.Backend.VULKAN, selection.backend)
        assertEquals("Vulkan session", selection.value)
        assertEquals(listOf(Engine.Backend.OPENGL, Engine.Backend.VULKAN), attempts)
    }

    @Test
    fun fallsBackToVulkanWhenOpenGlEsInitializationReturnsNull() {
        val attempts = mutableListOf<Engine.Backend>()
        val selection = createWithRendererBackendFallback { backend ->
            attempts += backend
            if (backend == Engine.Backend.OPENGL) null else "Vulkan session"
        }
        assertEquals(Engine.Backend.VULKAN, selection.backend)
        assertEquals("Vulkan session", selection.value)
        assertEquals(listOf(Engine.Backend.OPENGL, Engine.Backend.VULKAN), attempts)
    }

    @Test
    fun reportsFailuresWhenNeitherBackendInitializes() {
        try {
            createWithRendererBackendFallback<String> { backend ->
                throw IllegalStateException("${backend.displayName()} failed")
            }
            throw AssertionError("Expected both backend attempts to fail")
        } catch (failure: IllegalStateException) {
            assertTrue(failure.message!!.contains("OpenGL ES"))
            assertEquals(2, failure.suppressed.size)
        }
    }
}
