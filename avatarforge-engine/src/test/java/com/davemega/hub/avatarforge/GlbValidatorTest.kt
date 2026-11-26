package com.davemega.hub.avatarforge

import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.junit.Assert.assertThrows
import org.junit.Test

class GlbValidatorTest {
    @Test
    fun acceptsMinimalValidGlb() {
        val file = temporaryGlb(validGlb())
        try {
            GlbValidator.validate(file)
        } finally {
            file.delete()
        }
    }

    @Test
    fun rejectsBadMagicAndVersion() {
        val valid = validGlb()
        val badMagic = valid.copyOf().also { it[0] = 0 }
        val badVersion = valid.copyOf().also { it[4] = 1 }
        assertInvalid(badMagic)
        assertInvalid(badVersion)
    }

    @Test
    fun rejectsDeclaredLengthMismatchAndTruncatedChunk() {
        val mismatch = validGlb().also { it[8] = (it[8].toInt() - 4).toByte() }
        val truncated = validGlb().copyOf().also { it[12] = 0x7c }
        assertInvalid(mismatch)
        assertInvalid(truncated)
    }

    @Test
    fun rejectsMissingOrInvalidGlbJson() {
        assertInvalid(glbWithChunk(0x004E4942, byteArrayOf(0, 0, 0, 0)))
        val invalidJson = """{"asset":{"version":"1.0"}}""".toByteArray()
        assertInvalid(glbWithChunk(0x4E4F534A, pad4(invalidJson)))
    }

    private fun assertInvalid(bytes: ByteArray) {
        val file = temporaryGlb(bytes)
        try {
            assertThrows(IllegalArgumentException::class.java) { GlbValidator.validate(file) }
        } finally {
            file.delete()
        }
    }

    private fun temporaryGlb(bytes: ByteArray): File =
        File.createTempFile("mega-hub-glb-", ".glb").apply { writeBytes(bytes) }

    private fun validGlb(): ByteArray {
        val json = pad4("""{"asset":{"version":"2.0"}}""".toByteArray())
        return glbWithChunk(0x4E4F534A, json)
    }

    private fun glbWithChunk(type: Int, payload: ByteArray): ByteArray {
        val totalLength = 12 + 8 + payload.size
        return ByteBuffer.allocate(totalLength).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(0x46546C67)
            .putInt(2)
            .putInt(totalLength)
            .putInt(payload.size)
            .putInt(type)
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
