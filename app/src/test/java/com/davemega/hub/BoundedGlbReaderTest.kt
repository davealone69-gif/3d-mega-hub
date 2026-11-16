package com.davemega.hub

import java.io.ByteArrayInputStream
import java.io.File
import java.nio.file.Files
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class BoundedGlbReaderTest {
    @Test
    fun readsExactBytesAndRemovesTemporarySpool() {
        val cache = Files.createTempDirectory("mega-hub-glb-reader-").toFile()
        try {
            val expected = ByteArray(2 * 1024 * 1024) { (it % 251).toByte() }
            val actual = BoundedGlbReader.read(
                ByteArrayInputStream(expected),
                cache,
                expected.size.toLong()
            )

            assertArrayEquals(expected, actual)
            assertEquals(0, cache.listFiles()?.size ?: 0)
        } finally {
            cache.deleteRecursively()
        }
    }

    @Test
    fun rejectsOversizedInputAndRemovesTemporarySpool() {
        val cache = Files.createTempDirectory("mega-hub-glb-reader-").toFile()
        try {
            try {
                BoundedGlbReader.read(ByteArrayInputStream(byteArrayOf(1, 2, 3, 4, 5)), cache, 4L)
                fail("Expected the configured GLB safety limit to reject oversized input")
            } catch (expected: IllegalArgumentException) {
                assertTrue(expected.message?.contains("safety limit") == true)
            }
            assertEquals(0, cache.listFiles()?.size ?: 0)
        } finally {
            cache.deleteRecursively()
        }
    }

    @Test
    fun rejectsEmptyInputAndRemovesTemporarySpool() {
        val cache = Files.createTempDirectory("mega-hub-glb-reader-").toFile()
        try {
            try {
                BoundedGlbReader.read(ByteArrayInputStream(byteArrayOf()), cache, 16L)
                fail("Expected empty GLB input to be rejected")
            } catch (expected: IllegalArgumentException) {
                assertTrue(expected.message?.contains("empty") == true)
            }
            assertEquals(0, cache.listFiles()?.size ?: 0)
        } finally {
            cache.deleteRecursively()
        }
    }
}
