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
 * AvatarForge opens in the animation preview; everything else opens in the GLES renderer. The
 * user can still switch by hand in Avatar Studio.
 */
object GlbPreviewRouting {
    data class Summary(
        val generator: String,
        val animationCount: Int,
        val skinCount: Int,
        val morphTargetCount: Int
    )

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
        val meshes = root.optJSONArray("meshes")
        if (meshes != null) {
            for (m in 0 until meshes.length()) {
                val primitives = meshes.optJSONObject(m)?.optJSONArray("primitives") ?: continue
                for (p in 0 until primitives.length()) {
                    val targets = primitives.optJSONObject(p)?.optJSONArray("targets")?.length() ?: 0
                    if (targets > morphTargets) morphTargets = targets
                }
            }
        }
        return Summary(
            generator = root.optJSONObject("asset")?.optString("generator", "") ?: "",
            animationCount = root.optJSONArray("animations")?.length() ?: 0,
            skinCount = root.optJSONArray("skins")?.length() ?: 0,
            morphTargetCount = morphTargets
        )
    }

    /** True when the engine in this app wrote the file (its exporter stamps the generator). */
    fun isAvatarForgeExport(summary: Summary?): Boolean =
        summary?.generator?.startsWith("AvatarForge", ignoreCase = true) == true

    /** True: open in the Filament gltfio preview, which plays the file's clips. */
    fun opensInAnimationPreview(summary: Summary?): Boolean =
        summary != null && summary.animationCount > 0 && !isAvatarForgeExport(summary)

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
