package com.davemega.hub.avatarforge

import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Test

class EvidenceIdentityTest {
    private fun temporaryFile(bytes: ByteArray): File {
        val file = File.createTempFile("evidence-identity", ".bin")
        file.deleteOnExit()
        file.writeBytes(bytes)
        return file
    }

    @Test
    fun sha256MatchesThePublishedTestVectors() {
        // FIPS 180-4 test vectors for the empty message and for "abc".
        assertEquals(
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            EvidenceIdentity.sha256(temporaryFile(ByteArray(0)))
        )
        assertEquals(
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            EvidenceIdentity.sha256(temporaryFile("abc".toByteArray(Charsets.US_ASCII)))
        )
    }

    @Test
    fun sha256ReadsFilesLargerThanItsBuffer() {
        // One million times the letter "a" is the FIPS 180-4 long-message vector. It still fits in
        // one 1 MiB read, so a three-megabyte file is checked as well against the JDK digest.
        val million = ByteArray(1_000_000) { 'a'.code.toByte() }
        assertEquals(
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            EvidenceIdentity.sha256(temporaryFile(million))
        )
        val twoBlocks = temporaryFile(ByteArray(3_000_000) { (it % 251).toByte() })
        val expected = java.security.MessageDigest.getInstance("SHA-256")
            .digest(twoBlocks.readBytes())
            .joinToString("") { byte -> "%02x".format(byte.toInt() and 0xFF) }
        assertEquals(expected, EvidenceIdentity.sha256(twoBlocks))
    }

    @Test
    fun assetDigestIsEmptyForAMissingFileAndCachedForAnExistingOne() {
        val missing = File("definitely-missing-evidence-identity-test.glb")
        assertEquals("", EvidenceIdentity.assetSha256(missing))
        assertEquals("", EvidenceIdentity.assetSha256IfReady(missing))

        val file = temporaryFile("abc".toByteArray(Charsets.US_ASCII))
        assertEquals("", EvidenceIdentity.assetSha256IfReady(file))
        val digest = EvidenceIdentity.assetSha256(file)
        assertEquals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", digest)
        assertEquals(digest, EvidenceIdentity.assetSha256IfReady(file))
    }
}
