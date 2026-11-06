package com.davemega.hub.avatarforge

import java.io.BufferedInputStream
import java.io.ByteArrayInputStream
import java.io.File
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.CodingErrorAction
import java.nio.charset.StandardCharsets
import org.json.JSONArray
import org.json.JSONObject

/**
 * Bounded GLB validation at generation, import and renderer boundaries.
 * Filament can crash natively when primitives in one glTF mesh have different morph counts.
 */
object GlbValidator {
    private const val GLB_MAGIC = 0x46546C67L
    private const val JSON_CHUNK = 0x4E4F534AL
    private const val MAX_JSON_CHUNK_BYTES = 16L * 1024L * 1024L
    const val MAX_GLB_BYTES = 128L * 1024L * 1024L
    private val INDEX_COMPONENT_TYPES = setOf(5121, 5123, 5125)
    private val MORPH_SEMANTICS = setOf("POSITION", "NORMAL", "TANGENT")

    fun validate(file: File) {
        require(file.isFile) { "GLB file is missing." }
        val fileLength = file.length()
        require(fileLength in 20L..MAX_GLB_BYTES) { "GLB size is invalid: $fileLength bytes." }
        BufferedInputStream(file.inputStream()).use { validateContainer(it, fileLength) }
    }

    fun validate(bytes: ByteArray) {
        require(bytes.size.toLong() in 20L..MAX_GLB_BYTES) {
            "GLB byte array size is invalid: ${bytes.size} bytes."
        }
        ByteArrayInputStream(bytes).use { validateContainer(it, bytes.size.toLong()) }
    }

    private fun validateContainer(input: InputStream, fileLength: Long) {
        require(readU32(input) == GLB_MAGIC) { "File does not have the GLB magic header." }
        require(readU32(input) == 2L) { "GLB must use binary version 2." }
        require(readU32(input) == fileLength) { "GLB header length does not match the file size." }
        var offset = 12L
        var chunkIndex = 0
        var jsonFound = false
        while (offset < fileLength) {
            require(fileLength - offset >= 8L) { "GLB ends inside a chunk header." }
            val chunkLength = readU32(input)
            val chunkType = readU32(input)
            offset += 8L
            require(chunkLength % 4L == 0L) { "GLB contains a misaligned chunk." }
            require(chunkLength <= fileLength - offset) { "GLB contains a truncated chunk." }
            if (chunkIndex == 0) {
                require(chunkType == JSON_CHUNK) { "The first GLB chunk must contain JSON." }
                require(chunkLength in 2L..MAX_JSON_CHUNK_BYTES) { "GLB JSON chunk size is invalid." }
                val jsonBytes = ByteArray(chunkLength.toInt())
                input.readFully(jsonBytes)
                val jsonText = StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT)
                    .decode(ByteBuffer.wrap(jsonBytes))
                    .toString()
                    .trimEnd(' ', '\t', '\r', '\n')
                val root = try {
                    JSONObject(jsonText)
                } catch (failure: Exception) {
                    throw IllegalArgumentException("GLB JSON chunk is not a valid JSON object.", failure)
                }
                require(root.optJSONObject("asset")?.optString("version") == "2.0") {
                    "GLB JSON does not declare glTF asset version 2.0."
                }
                validateMeshCompatibility(root)
                jsonFound = true
            } else {
                skipFully(input, chunkLength)
            }
            offset += chunkLength
            chunkIndex++
        }
        require(jsonFound && chunkIndex > 0) { "GLB has no JSON chunk." }
        require(offset == fileLength) { "GLB contains trailing or unparsed bytes." }
    }

    private fun validateMeshCompatibility(root: JSONObject) {
        val accessors = root.optJSONArray("accessors") ?: JSONArray()
        val meshes = root.optJSONArray("meshes") ?: return
        val meshMorphCounts = IntArray(meshes.length())
        for (meshIndex in 0 until meshes.length()) {
            val mesh = meshes.optJSONObject(meshIndex)
                ?: throw IllegalArgumentException("GLB mesh[$meshIndex] is not an object.")
            val primitives = mesh.optJSONArray("primitives")
                ?: throw IllegalArgumentException("GLB mesh[$meshIndex] has no primitives array.")
            require(primitives.length() > 0) { "GLB mesh[$meshIndex] has no primitives." }
            var expectedMorphCount: Int? = null
            for (primitiveIndex in 0 until primitives.length()) {
                val primitive = primitives.optJSONObject(primitiveIndex)
                    ?: throw IllegalArgumentException("GLB mesh[$meshIndex] primitive[$primitiveIndex] is not an object.")
                val attributes = primitive.optJSONObject("attributes")
                    ?: throw IllegalArgumentException("GLB mesh[$meshIndex] primitive[$primitiveIndex] has no attributes.")
                val positionIndex = attributes.optInt("POSITION", -1)
                val position = requireAccessor(accessors, positionIndex, "mesh[$meshIndex] primitive[$primitiveIndex] POSITION")
                require(position.optString("type") == "VEC3") {
                    "GLB mesh[$meshIndex] primitive[$primitiveIndex] POSITION accessor must be VEC3."
                }
                val vertexCount = position.optLong("count", -1L)
                require(vertexCount > 0L) { "GLB mesh[$meshIndex] primitive[$primitiveIndex] has no vertices." }
                if (primitive.has("indices")) {
                    val index = requireAccessor(
                        accessors, primitive.optInt("indices", -1),
                        "mesh[$meshIndex] primitive[$primitiveIndex] indices"
                    )
                    require(index.optString("type") == "SCALAR") {
                        "GLB mesh[$meshIndex] primitive[$primitiveIndex] index accessor must be SCALAR."
                    }
                    require(index.optInt("componentType", -1) in INDEX_COMPONENT_TYPES) {
                        "GLB mesh[$meshIndex] primitive[$primitiveIndex] uses an invalid index component type."
                    }
                    require(index.optLong("count", -1L) > 0L) {
                        "GLB mesh[$meshIndex] primitive[$primitiveIndex] has zero indices."
                    }
                }
                val targets = primitive.optJSONArray("targets")
                val morphCount = targets?.length() ?: 0
                if (expectedMorphCount == null) expectedMorphCount = morphCount
                else require(expectedMorphCount == morphCount) {
                    "GLB mesh[$meshIndex] has inconsistent morph target count across primitives: " +
                        "expected $expectedMorphCount but primitive[$primitiveIndex] has $morphCount."
                }
                if (targets != null) {
                    for (targetIndex in 0 until targets.length()) {
                        val target = targets.optJSONObject(targetIndex)
                            ?: throw IllegalArgumentException(
                                "GLB mesh[$meshIndex] primitive[$primitiveIndex] target[$targetIndex] is not an object."
                            )
                        require(target.length() > 0) {
                            "GLB mesh[$meshIndex] primitive[$primitiveIndex] target[$targetIndex] is empty."
                        }
                        val keys = target.keys()
                        while (keys.hasNext()) {
                            val semantic = keys.next()
                            require(semantic in MORPH_SEMANTICS) {
                                "GLB morph target contains unsupported attribute '$semantic'."
                            }
                            val targetAccessor = requireAccessor(
                                accessors, target.optInt(semantic, -1),
                                "mesh[$meshIndex] primitive[$primitiveIndex] target[$targetIndex] $semantic"
                            )
                            require(targetAccessor.optString("type") == "VEC3" &&
                                targetAccessor.optLong("count", -1L) == vertexCount) {
                                "GLB mesh[$meshIndex] primitive[$primitiveIndex] target[$targetIndex] " +
                                    "$semantic accessor must be VEC3 with $vertexCount entries."
                            }
                        }
                    }
                }
            }
            val morphCount = expectedMorphCount ?: 0
            meshMorphCounts[meshIndex] = morphCount
            mesh.optJSONArray("weights")?.let { weights ->
                require(weights.length() == morphCount) {
                    "GLB mesh[$meshIndex] weights count does not match its $morphCount morph targets."
                }
            }
            mesh.optJSONObject("extras")?.optJSONArray("targetNames")?.let { names ->
                require(names.length() == morphCount) {
                    "GLB mesh[$meshIndex] targetNames count does not match its $morphCount morph targets."
                }
            }
        }
        root.optJSONArray("nodes")?.let { nodes ->
            for (nodeIndex in 0 until nodes.length()) {
                val node = nodes.optJSONObject(nodeIndex) ?: continue
                if (!node.has("mesh")) continue
                val meshIndex = node.optInt("mesh", -1)
                require(meshIndex in meshMorphCounts.indices) {
                    "GLB node[$nodeIndex] references missing mesh[$meshIndex]."
                }
                node.optJSONArray("weights")?.let { weights ->
                    require(weights.length() == meshMorphCounts[meshIndex]) {
                        "GLB node[$nodeIndex] weights count does not match mesh[$meshIndex] morph targets."
                    }
                }
            }
        }
    }

    private fun requireAccessor(accessors: JSONArray, index: Int, label: String): JSONObject {
        require(index in 0 until accessors.length()) { "GLB $label references missing accessor[$index]." }
        val accessor = accessors.optJSONObject(index)
            ?: throw IllegalArgumentException("GLB $label accessor[$index] is not an object.")
        require(accessor.optLong("count", -1L) >= 0L) { "GLB $label accessor[$index] has an invalid count." }
        return accessor
    }

    private fun readU32(input: InputStream): Long {
        val bytes = ByteArray(4)
        input.readFully(bytes)
        return ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xFFFF_FFFFL
    }

    private fun skipFully(input: InputStream, byteCount: Long) {
        var remaining = byteCount
        while (remaining > 0L) {
            val skipped = input.skip(remaining)
            if (skipped > 0L) remaining -= skipped
            else {
                require(input.read() != -1) { "GLB ended before a chunk was complete." }
                remaining--
            }
        }
    }

    private fun InputStream.readFully(bytes: ByteArray) {
        var offset = 0
        while (offset < bytes.size) {
            val count = read(bytes, offset, bytes.size - offset)
            require(count > 0) { "GLB ended unexpectedly." }
            offset += count
        }
    }
}
