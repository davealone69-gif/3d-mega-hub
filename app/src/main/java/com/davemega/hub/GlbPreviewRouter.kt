package com.davemega.hub

import android.net.Uri
import java.io.File
import java.io.InputStream
import org.json.JSONObject

/**
 * Decides whether a file-based GLB should open in the Filament gltfio preview by default or in the
 * native AvatarForge GLES viewport.
 *
 * Rules:
 * - AvatarForge outputs (by diagnostic/output filename or GLB `asset.generator` prefix) always
 *   route to `AvatarForgeSurfaceView` (native GLES). They carry 52 morph targets over dense meshes
 *   that exceed Filament gltfio's mobile morph buffer budget and are validated on the GLES path.
 * - External imported GLBs with embedded glTF animation clips (`animations.length() > 0`) and
 *   morph storage within budget open directly in `FilamentPreviewView` so skeletal animation can
 *   play immediately.
 * - Static external GLBs (`animations.length() == 0`) default to `AvatarForgeSurfaceView` (native
 *   GLES), while still allowing manual switching to Filament preview in the UI.
 */
object GlbPreviewRouter {
    internal data class GlbHeaderSummary(
        val generator: String,
        val animationCount: Int,
        val morphVertexTargets: Long
    )

    private const val MAGIC_GLTF = 0x46546C67L
    private const val CHUNK_JSON = 0x4E4F534AL
    private const val MAX_JSON_BYTES = 8L * 1024L * 1024L

    /** gltfio stores a position and tangent frame per vertex per morph target (~24 bytes each). */
    private const val MORPH_BYTES_PER_VERTEX_TARGET = 24L

    /** Cap default Filament preview routing to 96 MiB of gltfio morph target storage. */
    internal const val MAX_FILAMENT_MORPH_VERTEX_TARGETS =
        96L * 1024L * 1024L / MORPH_BYTES_PER_VERTEX_TARGET

    fun shouldDefaultToFilament(uri: Uri?): Boolean {
        if (uri == null || uri.scheme != "file") return false
        val path = uri.path ?: return false
        return shouldDefaultToFilament(File(path))
    }

    fun shouldDefaultToFilament(file: File?): Boolean {
        if (file == null || !file.isFile) return false
        if (isAvatarForgeOutputPath(file.path)) return false
        return runCatching {
            file.inputStream().buffered().use { stream ->
                shouldDefaultToFilament(file.name, stream)
            }
        }.getOrDefault(false)
    }

    internal fun shouldDefaultToFilament(fileName: String?, input: InputStream): Boolean {
        if (!fileName.isNullOrBlank() && isAvatarForgeOutputPath(fileName)) return false
        val summary = inspectGlbHeader(input) ?: return false
        if (isAvatarForgeGenerator(summary.generator)) return false
        if (summary.morphVertexTargets > MAX_FILAMENT_MORPH_VERTEX_TARGETS) return false
        return summary.animationCount > 0
    }

    internal fun isAvatarForgeOutputPath(path: String): Boolean {
        val name = path.substringAfterLast('/').substringAfterLast('\\')
        return name.contains("avatarforge", ignoreCase = true) ||
            name.contains("diagnostic", ignoreCase = true) ||
            name.endsWith("_generated.glb", ignoreCase = true)
    }

    internal fun isAvatarForgeGenerator(generator: String): Boolean =
        generator.startsWith("AvatarForge", ignoreCase = true)

    internal fun inspectGlbHeader(input: InputStream): GlbHeaderSummary? {
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

        val jsonBytes = ByteArray(jsonLength.toInt())
        if (!readFully(input, jsonBytes)) return null
        val root = runCatching { JSONObject(String(jsonBytes, Charsets.UTF_8)) }.getOrNull() ?: return null

        val generator = root.optJSONObject("asset")?.optString("generator", "") ?: ""
        val animationCount = root.optJSONArray("animations")?.length() ?: 0
        var morphVertexTargets = 0L
        val accessors = root.optJSONArray("accessors")
        val meshes = root.optJSONArray("meshes")
        if (meshes != null) {
            for (meshIndex in 0 until meshes.length()) {
                val primitives = meshes.optJSONObject(meshIndex)?.optJSONArray("primitives") ?: continue
                for (primitiveIndex in 0 until primitives.length()) {
                    val primitive = primitives.optJSONObject(primitiveIndex) ?: continue
                    val targets = primitive.optJSONArray("targets")?.length() ?: 0
                    if (targets <= 0) continue
                    val positionAccessor = primitive.optJSONObject("attributes")?.optInt("POSITION", -1) ?: -1
                    val vertexCount = if (positionAccessor >= 0) {
                        accessors?.optJSONObject(positionAccessor)?.optLong("count", 0L) ?: 0L
                    } else 0L
                    if (vertexCount > 0L) {
                        morphVertexTargets += vertexCount * targets
                    }
                }
            }
        }
        return GlbHeaderSummary(
            generator = generator,
            animationCount = animationCount,
            morphVertexTargets = morphVertexTargets
        )
    }

    private fun readFully(input: InputStream, target: ByteArray): Boolean {
        var offset = 0
        while (offset < target.size) {
            val count = input.read(target, offset, target.size - offset)
            if (count < 0) return false
            offset += count
        }
        return true
    }
}
