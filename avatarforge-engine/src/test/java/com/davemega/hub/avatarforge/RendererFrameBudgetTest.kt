package com.davemega.hub.avatarforge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RendererFrameBudgetTest {
    @Test
    fun keepsStrictBudgetOnHardwareGpus() {
        val xclipse = "gles31|OpenGL ES 3.2 v1.r44p1|Samsung Xclipse 940"
        val adreno = "gles31|OpenGL ES 3.2 V@0762.0|Adreno (TM) 750"
        assertFalse(RendererFrameBudget.isSoftwareRenderer(xclipse))
        assertFalse(RendererFrameBudget.isSoftwareRenderer(adreno))
        assertEquals(RendererFrameBudget.HARDWARE_BUDGET_NANOS, RendererFrameBudget.firstPresentBudgetNanos(xclipse))
        assertEquals(RendererFrameBudget.HARDWARE_BUDGET_NANOS, RendererFrameBudget.firstPresentBudgetNanos(adreno))
    }

    @Test
    fun extendsBudgetOnlyForSoftwareRasterisers() {
        val swiftShader = "gles31|OpenGL ES 3.1|Android Emulator OpenGL ES Translator (Google SwiftShader)"
        val angleSwiftShader = "gles31|OpenGL ES 3.1|ANGLE (Google, Vulkan 1.3.0 (SwiftShader Device), SwiftShader driver)"
        val llvmpipe = "gles31|OpenGL ES 3.2|llvmpipe (LLVM 15.0.7, 256 bits)"
        assertTrue(RendererFrameBudget.isSoftwareRenderer(swiftShader))
        assertTrue(RendererFrameBudget.isSoftwareRenderer(angleSwiftShader))
        assertTrue(RendererFrameBudget.isSoftwareRenderer(llvmpipe))
        assertEquals(RendererFrameBudget.SOFTWARE_BUDGET_NANOS, RendererFrameBudget.firstPresentBudgetNanos(swiftShader))
        assertEquals(RendererFrameBudget.SOFTWARE_BUDGET_NANOS, RendererFrameBudget.firstPresentBudgetNanos(angleSwiftShader))
        assertEquals(RendererFrameBudget.SOFTWARE_BUDGET_NANOS, RendererFrameBudget.firstPresentBudgetNanos(llvmpipe))
    }
}
