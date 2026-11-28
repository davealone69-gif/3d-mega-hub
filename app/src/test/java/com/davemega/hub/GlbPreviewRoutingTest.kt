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
    fun generatedAvatarIsNeverOfferedTheAnimationPreview() {
        // The file that ended the process on the phone: an avatar of this app, sent to gltfio by hand.
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"AvatarForge 0.10.1"},"skins":[{}],"animations":[{}],
               "accessors":[{"count":621802}],
               "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{},{}]}]}]}"""
        )
        assertNotNull(summary)
        assertEquals(621802L, summary!!.vertexCount)
        assertEquals(2L * 621802L, summary.morphVertexTargets)
        assertNotNull(GlbPreviewRouting.animationPreviewRefusal(summary))
        assertFalse(GlbPreviewRouting.mayOfferAnimationPreview(summary))
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(summary))
    }

    @Test
    fun foreignFileWithHugeMorphStorageIsRefused() {
        // 52 targets over 621 802 vertices is about 740 MB of morph storage in gltfio.
        val targets = (1..52).joinToString(",") { "{}" }
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"Some exporter"},"animations":[{}],
               "accessors":[{"count":621802}],
               "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[$targets]}]}]}"""
        )
        assertNotNull(summary)
        assertEquals(52, summary!!.morphTargetCount)
        assertTrue(summary.morphVertexTargets > GlbPreviewRouting.MAX_MORPH_VERTEX_TARGETS)
        val refusal = GlbPreviewRouting.animationPreviewRefusal(summary)
        assertNotNull(refusal)
        assertTrue(refusal!!.contains("MB"))
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(summary))
    }

    @Test
    fun smallForeignFileWithMorphTargetsIsStillAllowed() {
        // The Khronos AnimatedMorphCube: 24 vertices, 2 targets, 1 animation.
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"glTF Tools for Unity"},"animations":[{}],
               "accessors":[{"count":24}],
               "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{},{}]}]}]}"""
        )
        assertNotNull(summary)
        assertNull(GlbPreviewRouting.animationPreviewRefusal(summary))
        assertTrue(GlbPreviewRouting.mayOfferAnimationPreview(summary))
        assertTrue(GlbPreviewRouting.opensInAnimationPreview(summary))
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
    fun foreignModelBuiltFromSeveralTexturedPartsOpensInTheGltfViewer() {
        // The shape of the owner's "Heather" reference: no skeleton, no clips, parts with their own textures.
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"Sketchfab-16.61.0"},
               "textures":[{"source":0},{"source":1},{"source":2}],
               "materials":[
                 {"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},
                 {"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},
                 {"pbrMetallicRoughness":{"baseColorTexture":{"index":2}},"normalTexture":{"index":1}}]}"""
        )
        assertNotNull(summary)
        assertEquals(0, summary!!.animationCount)
        assertEquals(2, summary.baseColorImageCount)
        assertEquals(0, summary.transparentMaterialCount)
        assertTrue(GlbPreviewRouting.needsGltfViewer(summary))
        assertTrue(GlbPreviewRouting.opensInAnimationPreview(summary))
    }

    @Test
    fun foreignModelWithOneColourTextureStaysOnTheEngineRenderer() {
        // A single-texture scan: the GLES renderer shows all of it, and lights it.
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"Sketchfab-16.62.0"},
               "textures":[{"source":0},{"source":1}],
               "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":1}},
                             "extensions":{"KHR_materials_unlit":{}}}]}"""
        )
        assertNotNull(summary)
        assertEquals(1, summary!!.baseColorImageCount)
        assertFalse(GlbPreviewRouting.needsGltfViewer(summary))
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(summary))
        assertTrue(GlbPreviewRouting.mayOfferAnimationPreview(summary))
    }

    @Test
    fun transparentMaterialsNeedTheGltfViewer() {
        // Hair cards and lashes: the GLES renderer would draw them as solid sheets.
        val blend = summarize(
            """{"asset":{"version":"2.0","generator":"Blender"},"materials":[{"alphaMode":"BLEND"},{"alphaMode":"OPAQUE"},{}]}"""
        )
        assertEquals(1, blend!!.transparentMaterialCount)
        assertTrue(GlbPreviewRouting.opensInAnimationPreview(blend))
        val mask = summarize("""{"asset":{"version":"2.0","generator":"Blender"},"materials":[{"alphaMode":"MASK"}]}""")
        assertTrue(GlbPreviewRouting.opensInAnimationPreview(mask))
    }

    @Test
    fun generatedAvatarStaysOnTheNativeRendererWhateverItsMaterials() {
        val summary = summarize(
            """{"asset":{"version":"2.0","generator":"AvatarForge 0.10.1"},
               "textures":[{"source":0},{"source":1}],
               "materials":[{"alphaMode":"BLEND","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},
                            {"pbrMetallicRoughness":{"baseColorTexture":{"index":1}}}]}"""
        )
        assertTrue(GlbPreviewRouting.needsGltfViewer(summary))
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(summary))
        assertFalse(GlbPreviewRouting.mayOfferAnimationPreview(summary))
    }

    @Test
    fun modelWhoseTexturesDoNotFitIsRefusedByTheGltfViewer() {
        // The owner's Daz model as downloaded: 1,152 MiB of textures once decoded.
        val heavy = GlbPreviewRouting.Summary(
            generator = "Sketchfab-0.23.0", animationCount = 0, skinCount = 1, morphTargetCount = 0,
            vertexCount = 481553L, baseColorImageCount = 15, transparentMaterialCount = 15,
            decodedTextureBytes = 1152L * 1024L * 1024L
        )
        val refusal = GlbPreviewRouting.animationPreviewRefusal(heavy)
        assertNotNull(refusal)
        assertTrue(refusal!!.contains("1152 MB"))
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(heavy))
        assertFalse(GlbPreviewRouting.mayOfferAnimationPreview(heavy))
        // The same model after the import step halved its twelve largest textures.
        val fitted = heavy.copy(decodedTextureBytes = 384L * 1024L * 1024L)
        assertNull(GlbPreviewRouting.animationPreviewRefusal(fitted))
        assertTrue(GlbPreviewRouting.opensInAnimationPreview(fitted))
    }

    @Test
    fun summaryOfAFileIncludesItsDecodedTextureSize() {
        // A PNG header declaring 2048 x 2048, embedded as the only buffer view.
        val image = ByteArray(64)
        byteArrayOf(0x89.toByte(), 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A).copyInto(image)
        byteArrayOf(0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52, 0, 0, 8, 0, 0, 0, 8, 0).copyInto(image, 8)
        val json = """{"asset":{"version":"2.0","generator":"test"},"buffers":[{"byteLength":64}],
            "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":64}],"images":[{"bufferView":0,"mimeType":"image/png"}]}"""
            .toByteArray(Charsets.UTF_8)
        val padded = json + ByteArray((4 - json.size % 4) % 4) { ' '.code.toByte() }
        val buffer = ByteBuffer.allocate(20 + padded.size + 8 + image.size).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putInt(0x46546C67).putInt(2).putInt(20 + padded.size + 8 + image.size)
        buffer.putInt(padded.size).putInt(0x4E4F534A).put(padded)
        buffer.putInt(image.size).putInt(0x004E4942).put(image)
        val file = java.io.File.createTempFile("routing", ".glb")
        try {
            file.writeBytes(buffer.array())
            val summary = GlbPreviewRouting.summarize(file)
            assertNotNull(summary)
            assertEquals(GlbTextureBudget.decodedBytes(2048, 2048), summary!!.decodedTextureBytes)
            assertEquals(0L, GlbPreviewRouting.summarize(ByteArrayInputStream(buffer.array()))!!.decodedTextureBytes)
        } finally {
            file.delete()
        }
    }

    @Test
    fun unreadableInputIsNotRoutedToTheAnimationPreview() {
        assertNull(GlbPreviewRouting.summarize(ByteArrayInputStream(ByteArray(8))))
        assertNull(GlbPreviewRouting.summarize(ByteArrayInputStream("not a glb, not even close....".toByteArray())))
        assertFalse(GlbPreviewRouting.opensInAnimationPreview(null))
    }
}
