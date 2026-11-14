package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.net.URI

class RemoteGlbUrlTest {
    @Test
    fun rewritesDropboxShareLinkDlZeroToDirectDownloadPreservingRlkey() {
        val input =
            "https://www.dropbox.com/scl/fi/kib2ndd9ec1bwkza8jc0j/mylove.glb?rlkey=kn1mngyrmlir2p6650xwxo1pv&st=rv0d7501&dl=0"
        val normalized = RemoteGlbUrl.normalizeDownloadUrl(input)
        assertEquals(
            "https://www.dropbox.com/scl/fi/kib2ndd9ec1bwkza8jc0j/mylove.glb?rlkey=kn1mngyrmlir2p6650xwxo1pv&st=rv0d7501&dl=1",
            normalized.toString()
        )
        assertEquals("mylove.glb", RemoteGlbUrl.deriveDisplayName(normalized))
    }

    @Test
    fun extractsUrlFromSharedTextAndAddsDlOneWhenMissing() {
        val shared =
            "Open my model https://www.dropbox.com/s/abc123xyz/avatar_mesh.glb?raw=0."
        val normalized = RemoteGlbUrl.normalizeDownloadUrl(shared)
        assertEquals("https://www.dropbox.com/s/abc123xyz/avatar_mesh.glb?dl=1", normalized.toString())
        assertEquals("avatar_mesh.glb", RemoteGlbUrl.deriveDisplayName(normalized))
    }

    @Test
    fun rejectsCleartextRemoteHttpAndDowngradeRedirects() {
        assertThrows(IllegalArgumentException::class.java) {
            RemoteGlbUrl.normalizeDownloadUrl("http://example.com/model.glb")
        }
        val origin = URI("https://www.dropbox.com/scl/fi/id/mylove.glb?dl=1")
        assertThrows(IllegalArgumentException::class.java) {
            RemoteGlbUrl.validateRedirectTarget(origin, "http://dl.dropboxusercontent.com/mylove.glb")
        }
        assertThrows(IllegalArgumentException::class.java) {
            RemoteGlbUrl.validateRedirectTarget(origin, "https://localhost:8443/mylove.glb")
        }
    }
}

