package com.davemega.hub.avatarforge

import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Guards the structural clip inventory that Avatar Studio uses to explain why the Filament preview
 * shows no animation controls. A file that declares zero clips can never produce controls, so that
 * case must be distinguishable from a viewer that failed to publish the clips it does have.
 */
class GlbValidatorAnimationInventoryTest {
    @Test
    fun reportsDeclaredClipNamesAndSkinCount() {
        val json = """
            {"asset":{"version":"2.0"},
             "animations":[{"name":"Walk","channels":[]},{"name":"Idle","channels":[]}],
             "skins":[{"joints":[1,2]}]}
        """.trimIndent()

        val inventory = GlbValidator.inspectAnimations(glb(json))

        assertEquals(2, inventory.animationCount)
        assertEquals(listOf("Walk", "Idle"), inventory.animationNames)
        assertEquals(1, inventory.skinCount)
        assertTrue(inventory.declaresClips)
    }

    @Test
    fun reportsNoClipsForStaticAsset() {
        val inventory = GlbValidator.inspectAnimations(
            glb("""{"asset":{"version":"2.0"},"meshes":[]}""")
        )

        assertEquals(0, inventory.animationCount)
        assertTrue(inventory.animationNames.isEmpty())
        assertEquals(0, inventory.skinCount)
        assertFalse(inventory.declaresClips)
    }

    @Test
    fun namesUnnamedClipsByOneBasedIndex() {
        val json = """{"asset":{"version":"2.0"},"animations":[{"channels":[]},{"name":"   ","channels":[]}]}"""

        val inventory = GlbValidator.inspectAnimations(glb(json))

        assertEquals(listOf("clip 1", "clip 2"), inventory.animationNames)
    }

    @Test
    fun readsTheSameInventoryFromAFileAndFromBytes() {
        val bytes = glb(
            """{"asset":{"version":"2.0"},"animations":[{"name":"Wave","channels":[]}],"skins":[{"joints":[3]},{"joints":[4]}]}"""
        )
        val file = File.createTempFile("mega-hub-glb-animation-", ".glb").apply { writeBytes(bytes) }
        try {
            assertEquals(GlbValidator.inspectAnimations(bytes), GlbValidator.inspectAnimations(file))
            assertEquals(2, GlbValidator.inspectAnimations(file).skinCount)
        } finally {
            file.delete()
        }
    }

    @Test
    fun rejectsAFileThatIsNotGlbV2() {
        val badMagic = glb("""{"asset":{"version":"2.0"}}""").also { it[0] = 0 }
        val badVersion = glb("""{"asset":{"version":"2.0"}}""").also { it[4] = 1 }

        assertInventoryRejected(badMagic)
        assertInventoryRejected(badVersion)
    }

    @Test
    fun rejectsATruncatedJsonChunk() {
        assertInventoryRejected(glb("""{"asset":{"version":"2.0"}}""").also { it[12] = 0x7c })
    }

    private fun assertInventoryRejected(bytes: ByteArray) {
        assertThrows(IllegalArgumentException::class.java) { GlbValidator.inspectAnimations(bytes) }
    }

    private fun glb(jsonText: String): ByteArray {
        val payload = pad4(jsonText.toByteArray(Charsets.UTF_8))
        val totalLength = 12 + 8 + payload.size
        return ByteBuffer.allocate(totalLength).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(0x46546C67)
            .putInt(2)
            .putInt(totalLength)
            .putInt(payload.size)
            .putInt(0x4E4F534A)
            .put(payload)
            .array()
    }

    private fun pad4(bytes: ByteArray): ByteArray {
        val size = (bytes.size + 3) and 3.inv()
        return bytes.copyOf(size).also { padded ->
            for (index in bytes.size until size) padded[index] = 0x20
        }
    }
}
