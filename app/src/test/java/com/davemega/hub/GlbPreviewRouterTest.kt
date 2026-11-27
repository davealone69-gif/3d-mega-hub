package com.davemega.hub

import java.io.ByteArrayInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GlbPreviewRouterTest {
    private fun makeGlbBytes(json: String): ByteArray {
        val rawJson = json.toByteArray(Charsets.UTF_8)
        val paddedJsonLen = (rawJson.size + 3) and -4
        val totalLen = 12 + 8 + paddedJsonLen
        val buffer = ByteBuffer.allocate(totalLen).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putInt(0x46546C67) // glTF
        buffer.putInt(2)          // version 2
        buffer.putInt(totalLen)
        buffer.putInt(paddedJsonLen)
        buffer.putInt(0x4E4F534A) // JSON
        buffer.put(rawJson)
        repeat(paddedJsonLen - rawJson.size) { buffer.put(0x20) }
        return buffer.array()
    }

    @Test
    fun routesAvatarForgeGeneratedExportsToGlesEvenWithAnimations() {
        val glb = makeGlbBytes(
            """{"asset":{"version":"2.0","generator":"AvatarForge 0.1.0"},"animations":[{"name":"idle"}]}"""
        )
        assertFalse(
            "AvatarForge exports must route to native GLES rather than Filament",
            GlbPreviewRouter.shouldDefaultToFilament("avatar-1337-1730000000.glb", ByteArrayInputStream(glb))
        )
    }

    @Test
    fun routesDiagnosticAndAvatarForgeFilenamesToGles() {
        val animatedExternal = makeGlbBytes(
            """{"asset":{"version":"2.0","generator":"Blender"},"animations":[{"name":"walk"}]}"""
        )
        assertFalse(
            GlbPreviewRouter.shouldDefaultToFilament(
                "diagnostic-seed-1337-1730000000.glb",
                ByteArrayInputStream(animatedExternal)
            )
        )
        assertFalse(
            GlbPreviewRouter.shouldDefaultToFilament(
                "avatarforge-renderer-smoke.glb",
                ByteArrayInputStream(animatedExternal)
            )
        )
        assertFalse(
            GlbPreviewRouter.shouldDefaultToFilament(
                "custom_generated.glb",
                ByteArrayInputStream(animatedExternal)
            )
        )
    }

    @Test
    fun routesExternalAnimatedGlbWithinBudgetToFilament() {
        val animatedExternal = makeGlbBytes(
            """{"asset":{"version":"2.0","generator":"Khronos glTF Blender I/O"},"animations":[{"name":"wave"}],"accessors":[{"count":12000}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{},{}]}]}]}"""
        )
        assertTrue(
            "External animated GLBs within morph budget should open in Filament preview",
            GlbPreviewRouter.shouldDefaultToFilament("mylove_rig_prepared.glb", ByteArrayInputStream(animatedExternal))
        )
    }

    @Test
    fun routesStaticExternalGlbToGlesByDefault() {
        val staticExternal = makeGlbBytes(
            """{"asset":{"version":"2.0","generator":"Khronos glTF Blender I/O"},"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}]}"""
        )
        assertFalse(
            "Static external GLBs with zero animations should default to native GLES",
            GlbPreviewRouter.shouldDefaultToFilament("mylove.glb", ByteArrayInputStream(staticExternal))
        )
    }

    @Test
    fun refusesFilamentDefaultWhenMorphTargetStorageExceedsMobileBudget() {
        val heavyMorphs = makeGlbBytes(
            """{"asset":{"version":"2.0","generator":"ExternalExporter"},"animations":[{"name":"talk"}],"accessors":[{"count":500000}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{},{},{},{},{},{},{},{},{},{}]}]}]}"""
        )
        assertFalse(
            "Dense morph-target GLBs exceeding 96 MiB gltfio budget must not default to Filament",
            GlbPreviewRouter.shouldDefaultToFilament("dense_head.glb", ByteArrayInputStream(heavyMorphs))
        )
    }

    @Test
    fun rejectsMalformedBytesSafelyToGles() {
        val invalid = byteArrayOf(0, 1, 2, 3, 4)
        assertFalse(GlbPreviewRouter.shouldDefaultToFilament("broken.glb", ByteArrayInputStream(invalid)))
    }
}
