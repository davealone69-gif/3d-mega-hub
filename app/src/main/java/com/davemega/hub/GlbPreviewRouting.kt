package com.davemega.hub

import java.io.File
import java.io.InputStream
import org.json.JSONObject

/**
 * Decides which preview a file-based GLB opens in, from what the file contains.
 *
 * There are two previews and neither can replace the other today:
 *  - the Filament gltfio preview plays the animation clips a GLB carries, but it cannot hold an
 *    avatar this app generated (about a million triangles, each primitive with 52 morph targets);
 *  - the AvatarForge GLES renderer draws those avatars, and draws imported files as static models.
 *
 * So the rule is the narrowest one that is true: a file that carries clips and was not written by
 * AvatarForge opens in the animation preview; everything else opens in the GLES renderer.
 *
 * The animation preview is never given a file it cannot hold. Until 2026-11-15 Avatar Studio let
 * the user send any file there by hand, and sending a generated avatar - the only way the app
 * offered to see one move - ended the process: gltfio reserves morph storage for every vertex of
 * every target, several hundred megabytes for such an avatar. [animationPreviewRefusal] is asked
 * before the switch is offered and again inside the preview itself. Avatars made here are animated
 * by the GLES renderer instead, which skins them on the GPU.
 */
object GlbPreviewRouting {
    data class Summary(
        val generator: String,
        val animationCount: Int,
        val skinCount: Int,
        val morphTargetCount: Int,
        /** Vertices over all primitives, from the POSITION accessors. */
        val vertexCount: Long = 0L,
        /** Sum over primitives of vertices x morph targets: what gltfio sizes its morph storage by. */
        val morphVertexTargets: Long = 0L
    )

    /** gltfio keeps a position and a tangent frame per vertex per target: about 24 bytes each. */
    const val MORPH_BYTES_PER_VERTEX_TARGET = 24L

    /** The most morph storage the animation preview is allowed to ask a phone for: 96 MiB. */
    const val MAX_MORPH_VERTEX_TARGETS = 96L * 1024L * 1024L / MORPH_BYTES_PER_VERTEX_TARGET

    /** Largest JSON chunk read for the decision; a generated avatar's is well under 1 MB. */
    private const val MAX_JSON_BYTES = 8L * 1024L * 1024L
    private const val MAGIC_GLTF = 0x46546C67L
    private const val CHUNK_JSON = 0x4E4F534AL

    /** Reads only the GLB header and JSON chunk. Returns null when the file is not a readable GLB. */
    fun summarize(file: File): Summary? =
        runCatching { file.inputStream().buffered().use { summarize(it) } }.getOrNull()

    fun summarize(input: InputStream): Summary? {
        val header = ByteArray(20)
        if (!readFully(input, header)) return null
        fun u32(offset: Int): Long =
            (header[offset].toLong() and 0xFF) or
                ((header[offset + 1].toLong() and 0xFF) shl 8) or
                ((header[offset + 2].toLong() and 0xFF) shl 16) or
                ((header[offset + 3].toLong() and 0xFF) shl 24)
        if (u32(0) != MAGIC_GLTF || u32(4) != 2L) return null
        val jsonLength = u32(12)
        if (u32(16) != CHUNK_JSON || jsonLength <= 0L || jsonLength > MAX_JSON_BYTES) return null
        val json = ByteArray(jsonLength.toInt())
        if (!readFully(input, json)) return null
        val root = runCatching { JSONObject(String(json, Charsets.UTF_8)) }.getOrNull() ?: return null
        var morphTargets = 0
        var vertices = 0L
        var morphVertexTargets = 0L
        val accessors = root.optJSONArray("accessors")
        val meshes = root.optJSONArray("meshes")
        if (meshes != null) {
            for (m in 0 until meshes.length()) {
                val primitives = meshes.optJSONObject(m)?.optJSONArray("primitives") ?: continue
                for (p in 0 until primitives.length()) {
                    val primitive = primitives.optJSONObject(p) ?: continue
                    val targets = primitive.optJSONArray("targets")?.length() ?: 0
                    if (targets > morphTargets) morphTargets = targets
                    val position = primitive.optJSONObject("attributes")?.optInt("POSITION", -1) ?: -1
                    val count = if (position >= 0) accessors?.optJSONObject(position)?.optLong("count", 0L) ?: 0L else 0L
                    if (count > 0L) {
                        vertices += count
                        morphVertexTargets += count * targets
                    }
                }
            }
        }
        return Summary(
            generator = root.optJSONObject("asset")?.optString("generator", "") ?: "",
            animationCount = root.optJSONArray("animations")?.length() ?: 0,
            skinCount = root.optJSONArray("skins")?.length() ?: 0,
            morphTargetCount = morphTargets,
            vertexCount = vertices,
            morphVertexTargets = morphVertexTargets
        )
    }

    /** True when the engine in this app wrote the file (its exporter stamps the generator). */
    fun isAvatarForgeExport(summary: Summary?): Boolean =
        summary?.generator?.startsWith("AvatarForge", ignoreCase = true) == true

    /**
     * Why the Filament gltfio preview must not be given this file, in words for the user; null
     * when it may be. A file that could not be summarised is not refused here: the preview's own
     * reader validates it.
     */
    fun animationPreviewRefusal(summary: Summary?): String? = when {
        summary == null -> null
        isAvatarForgeExport(summary) ->
            "it is an avatar made by this app, which the animation preview cannot hold. " +
                "Its animations play in the main view: use the Motion controls"
        summary.morphVertexTargets > MAX_MORPH_VERTEX_TARGETS ->
            "its ${summary.morphTargetCount} morph targets over ${summary.vertexCount} vertices need about " +
                "${summary.morphVertexTargets * MORPH_BYTES_PER_VERTEX_TARGET / (1024L * 1024L)} MB in the animation preview"
        else -> null
    }

    /** True: open in the Filament gltfio preview, which plays the file's clips. */
    fun opensInAnimationPreview(summary: Summary?): Boolean =
        summary != null && summary.animationCount > 0 && animationPreviewRefusal(summary) == null

    /** True: the user may be offered the animation preview for this file by hand. */
    fun mayOfferAnimationPreview(summary: Summary?): Boolean =
        summary != null && animationPreviewRefusal(summary) == null

    private fun readFully(input: InputStream, target: ByteArray): Boolean {
        var read = 0
        while (read < target.size) {
            val count = input.read(target, read, target.size - read)
            if (count < 0) return false
            read += count
        }
        return true
    }
}
