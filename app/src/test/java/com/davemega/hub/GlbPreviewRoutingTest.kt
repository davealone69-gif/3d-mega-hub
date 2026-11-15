package com.davemega.hub

import java.io.ByteArrayInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class GlbPreviewRoutingTest {
    /** A GLB container holding only a JSON chunk: all the routing decision reads. */
    private fun glb(json: String): ByteArray {
        val raw = json.toByteArray(Charsets.UTF_8)
        val padded = raw + ByteArray((4 - raw.size % 4) % 4) { ' '.code.toByte() }
        val buffer = ByteBuffer.allocate(20 + padded.size).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putInt(0x46546C67).putInt(2).putInt(20 + padded.size)
        buffer.putInt(padded.size).putInt(0x4E4F534A).put(padded)
        return buffer.array()
    }

    private fun summarize(json: String) = GlbPreviewRouting.summarize(ByteArrayInputStream(glb(json)))

    @Test
    fun foreignFileWithClipsOpensInTheAnimationPreview() {
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"Khronos glTF Blender I/O"},"skins":[{}],"animations":[{},{}]}"""
        )
        assertNotNull(summary)
        assertEquals(2, summary!!.animationCount)
        assertEquals(1, summary.skinCount)
        assertTrue(GlbPreviewRouting.opensInAnimationPreview(summary))
    }

    @Test
    fun generatedAvatarStaysOnTheNativeRendererEvenWithClips() {
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"AvatarForge 0.10.1"},"skins":[{}],"animations":[{},{},{}],
               "meshes":[{"primitives":[{"targets":[{},{},{}]},{"targets":[{}]}]}]}"""
        )
        assertNotNull(summary)
        assertTrue(GlbPreviewRouting.isAvatarForgeExport(summary))
        assertEquals(3, summary!!.morphTargetCount)
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(summary))
    }

    @Test
    fun riggedFileWithoutClipsHasNothingToPlay() {
        // The shape of the owner's prepared reference avatar: a skin, joints and weights, no clips.
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"https://github.com/mikedh/trimesh"},"skins":[{}]}"""
        )
        assertNotNull(summary)
        assertEquals(0, summary!!.animationCount)
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(summary))
    }

    @Test
    fun unreadableInputIsNotRoutedToTheAnimationPreview() {
        assertNull(GlbPreviewRouting.summarize(ByteArrayInputStream(ByteArray(8))))
        assertNull(GlbPreviewRouting.summarize(ByteArrayInputStream("not a glb, not even close....".toByteArray())))
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(null))
    }
}
