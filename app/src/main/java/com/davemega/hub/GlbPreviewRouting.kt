package com.davemega.hub

import java.io.File
import java.io.InputStream
import org.json.JSONObject

/**
 * Decides which preview a file-based GLB opens in, from what the file contains.
 *
 * There are two previews and neither can replace the other today:
 *  - the Filament gltfio preview draws any glTF model as its author made it: every material with
 *    its own textures, transparency, the file's skeleton and its animation clips. It cannot hold an
 *    avatar this app generated (about a million triangles, each primitive with 52 morph targets);
 *  - the AvatarForge GLES renderer draws those avatars and animates them. Of an imported file it
 *    keeps ONE colour texture for the whole model and draws everything opaque and static.
 *
 * So the rule is the narrowest one that is true: a file that was not written by AvatarForge and
 * that the GLES renderer cannot show as it is - it carries clips, or more than one colour texture,
 * or a transparent material - opens in the gltfio preview; everything else opens in the GLES
 * renderer. Until 2026-11-15 only clips decided this, and a model built from many textured parts
 * came up mostly grey: of the owner's reference models, one showed 2 of its 14 parts textured and
 * another 4 of 45.
 *
 * The gltfio preview is never given a file it cannot hold. Two things are checked, both from the
 * JSON chunk and the image headers, before a byte of the model is loaded:
 *  - morph storage. gltfio reserves a position and a tangent frame for every vertex of every
 *    target. Sending a generated avatar there - until 2026-11-15 the only way the app offered to
 *    see one move - ended the process;
 *  - texture memory. gltfio decodes every image to RGBA with a mip chain. A desktop model with a
 *    dozen 4096 x 4096 textures needs more than a gigabyte. [GlbTextureBudget] reduces such a file
 *    at import; a file that is still over the limit is refused.
 * [animationPreviewRefusal] is asked before the switch is offered and again inside the preview.
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
        val morphVertexTargets: Long = 0L,
        /** Different images used as a base colour texture by some material. */
        val baseColorImageCount: Int = 0,
        /** Materials whose alpha mode is BLEND or MASK. */
        val transparentMaterialCount: Int = 0,
        /** RGBA8 with mip chains over all embedded images; 0 when the sizes were not read. */
        val decodedTextureBytes: Long = 0L
    )

    /** gltfio keeps a position and a tangent frame per vertex per target: about 24 bytes each. */
    const val MORPH_BYTES_PER_VERTEX_TARGET = 24L

    /** The most morph storage the animation preview is allowed to ask a phone for: 96 MiB. */
    const val MAX_MORPH_VERTEX_TARGETS = 96L * 1024L * 1024L / MORPH_BYTES_PER_VERTEX_TARGET

    /** Largest JSON chunk read for the decision; a generated avatar's is well under 1 MB. */
    private const val MAX_JSON_BYTES = 8L * 1024L * 1024L
    private const val MAGIC_GLTF = 0x46546C67L
    private const val CHUNK_JSON = 0x4E4F534AL

    /**
     * Reads the GLB header, the JSON chunk and the first bytes of each embedded image (for its
     * pixel size). Returns null when the file is not a readable GLB.
     */
    fun summarize(file: File): Summary? {
        val summary = runCatching { file.inputStream().buffered().use { summarize(it) } }.getOrNull() ?: return null
        // An avatar made here never goes to the gltfio preview, so its texture sizes decide nothing.
        if (isAvatarForgeExport(summary)) return summary
        val decoded = GlbTextureBudget.inspect(file)?.sumOf { it.decodedBytes } ?: 0L
        return summary.copy(decodedTextureBytes = decoded)
    }

    /** Reads only the GLB header and JSON chunk: texture sizes are not known from a stream. */

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
        val textures = root.optJSONArray("textures")
        val materials = root.optJSONArray("materials")
        val baseColorImages = HashSet<Int>()
        var transparentMaterials = 0
        if (materials != null) {
            for (m in 0 until materials.length()) {
                val material = materials.optJSONObject(m) ?: continue
                val alphaMode = material.optString("alphaMode", "OPAQUE")
                if (alphaMode == "BLEND" || alphaMode == "MASK") transparentMaterials++
                val texture = material.optJSONObject("pbrMetallicRoughness")
                    ?.optJSONObject("baseColorTexture")?.optInt("index", -1) ?: -1
                if (texture < 0) continue
                // The image behind the texture; a texture without a plain source still counts as one.
                val image = textures?.optJSONObject(texture)?.optInt("source", -1) ?: -1
                baseColorImages += if (image >= 0) image else -1 - texture
            }
        }
        return Summary(
            generator = root.optJSONObject("asset")?.optString("generator", "") ?: "",
            animationCount = root.optJSONArray("animations")?.length() ?: 0,
            skinCount = root.optJSONArray("skins")?.length() ?: 0,
            morphTargetCount = morphTargets,
            vertexCount = vertices,
            morphVertexTargets = morphVertexTargets,
            baseColorImageCount = baseColorImages.size,
            transparentMaterialCount = transparentMaterials
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
        summary.decodedTextureBytes > GlbTextureBudget.MAX_DECODED_TEXTURE_BYTES ->
            "its textures need about ${summary.decodedTextureBytes / (1024L * 1024L)} MB once decoded, and this viewer " +
                "holds at most ${GlbTextureBudget.MAX_DECODED_TEXTURE_BYTES / (1024L * 1024L)} MB"
        else -> null
    }

    /**
     * True when the GLES renderer cannot show the file as its author made it: it keeps one colour
     * texture for a whole imported model and draws every material opaque.
     */
    fun needsGltfViewer(summary: Summary?): Boolean =
        summary != null && (summary.baseColorImageCount > 1 || summary.transparentMaterialCount > 0)

    /** True: open in the Filament gltfio preview, which plays the file's clips and draws all its materials. */
    fun opensInAnimationPreview(summary: Summary?): Boolean =
        summary != null && animationPreviewRefusal(summary) == null &&
            (summary.animationCount > 0 || needsGltfViewer(summary))

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
