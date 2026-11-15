package com.davemega.hub

import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream
import java.io.RandomAccessFile
import org.json.JSONArray
import org.json.JSONObject

/**
 * Keeps the textures of an imported GLB inside what the phone can hold.
 *
 * A model from a desktop tool can carry a dozen 4096 x 4096 textures. Decoded to RGBA with a mip
 * chain that is 85 MiB each: the first such file the owner supplied needs 1,152 MiB, which neither
 * preview can be asked for. This reads the size of every embedded image from its header, decides
 * the smallest reduction that fits [MAX_DECODED_TEXTURE_BYTES], and writes a copy of the file in
 * which only those images are replaced. Geometry, skins, animations and every other byte of the
 * model are copied unchanged.
 *
 * The class does not decode or encode images itself: [Scaler] does, so the layout logic runs in
 * plain JVM tests and the Android bitmap code stays in one small place.
 */
object GlbTextureBudget {
    /** RGBA8 with a full mip chain, summed over the images of one model: 448 MiB. */
    const val MAX_DECODED_TEXTURE_BYTES = 448L * 1024L * 1024L

    private const val MAGIC_GLTF = 0x46546C67L
    private const val CHUNK_JSON = 0x4E4F534AL
    private const val CHUNK_BIN = 0x004E4942L
    private const val MAX_JSON_BYTES = 16L * 1024L * 1024L
    private const val HEADER_PROBE_BYTES = 64 * 1024
    private const val LONG_HEADER_PROBE_BYTES = 1024 * 1024
    private const val MAX_IMAGE_BYTES = 96L * 1024L * 1024L
    private val SIDE_LIMITS = intArrayOf(4096, 2048, 1024, 512, 256)

    const val MIME_PNG = "image/png"
    const val MIME_JPEG = "image/jpeg"
    const val MIME_WEBP = "image/webp"

    /** One entry of the file's `images` array. [mimeType] is what the bytes are, null when unknown. */
    data class ImageInfo(
        val index: Int,
        val bufferView: Int,
        val mimeType: String?,
        val width: Int,
        val height: Int,
        val byteLength: Long
    ) {
        val decodedBytes: Long get() = decodedBytes(width, height)
    }

    /**
     * [divisors] maps an image index to the power of two its sides are divided by. Images that are
     * not in the map stay as they are.
     */
    data class Plan(
        val images: List<ImageInfo>,
        val divisors: Map<Int, Int>,
        val decodedBefore: Long,
        val decodedAfter: Long,
        val sideLimit: Int,
        val budgetBytes: Long
    ) {
        val needsRewrite: Boolean get() = divisors.isNotEmpty()
        val fitsBudget: Boolean get() = decodedAfter <= budgetBytes
    }

    class Scaled(val bytes: ByteArray, val mimeType: String)

    /** Decodes [encoded], divides both sides by [divisor] and encodes it again; null when it cannot. */
    fun interface Scaler {
        fun scale(encoded: ByteArray, mimeType: String, divisor: Int, lossless: Boolean): Scaled?
    }

    /** What a renderer allocates for a width x height RGBA8 texture with all its mip levels (4/3 of the top level). */
    fun decodedBytes(width: Int, height: Int): Long =
        if (width <= 0 || height <= 0) 0L else (width.toLong() * height.toLong() * 16L + 2L) / 3L

    /** The images of [file] with their pixel sizes, or null when the file is not a readable GLB. */
    fun inspect(file: File): List<ImageInfo>? = try {
        RandomAccessFile(file, "r").use { source ->
            readContainer(source)?.let { imagesOf(it, source) }
        }
    } catch (_: Exception) {
        null
    }

    /**
     * The smallest reduction that brings the decoded size under [budgetBytes]: every PNG or JPEG
     * whose longer side exceeds a limit is halved until it does not, and the limit drops from 4096
     * to 256 until the sum fits. A model that is already inside the budget gets an empty plan.
     */
    fun plan(images: List<ImageInfo>, budgetBytes: Long = MAX_DECODED_TEXTURE_BYTES): Plan {
        val before = images.sumOf { it.decodedBytes }
        if (before <= budgetBytes) return Plan(images, emptyMap(), before, before, 0, budgetBytes)
        var chosen = Plan(images, emptyMap(), before, before, 0, budgetBytes)
        for (limit in SIDE_LIMITS) {
            val divisors = LinkedHashMap<Int, Int>()
            var total = 0L
            for (image in images) {
                var divisor = 1
                if (isScalable(image)) {
                    while (maxOf(image.width, image.height) / divisor > limit) divisor *= 2
                }
                if (divisor > 1) divisors[image.index] = divisor
                total += decodedBytes(maxOf(1, image.width / divisor), maxOf(1, image.height / divisor))
                    .let { if (image.width <= 0 || image.height <= 0) 0L else it }
            }
            chosen = Plan(images, divisors, before, total, limit, budgetBytes)
            if (total <= budgetBytes) break
        }
        return chosen
    }

    /** "12 of 18 textures reduced to at most 2048 pixels a side: 1152 MB decoded before, 384 MB after". */
    fun describe(plan: Plan): String {
        val megabyte = 1024L * 1024L
        return if (!plan.needsRewrite) {
            "${plan.images.size} textures, ${plan.decodedBefore / megabyte} MB decoded"
        } else {
            "${plan.divisors.size} of ${plan.images.size} textures reduced to at most ${plan.sideLimit} pixels a side: " +
                "${plan.decodedBefore / megabyte} MB decoded before, ${plan.decodedAfter / megabyte} MB after"
        }
    }

    /**
     * Writes [target] as a copy of [source] in which the images named by [plan] are replaced by
     * what [scaler] returns for them. Returns false, leaving no usable target, when the file is laid
     * out in a way this does not rewrite (more than one buffer, an external buffer, meshopt
     * compression, overlapping buffer views) or when no image could be scaled.
     */
    fun rewrite(source: File, target: File, plan: Plan, scaler: Scaler): Boolean {
        if (!plan.needsRewrite) return false
        RandomAccessFile(source, "r").use { input ->
            val container = readContainer(input) ?: return false
            val root = container.json
            val views = root.optJSONArray("bufferViews") ?: return false
            val images = root.optJSONArray("images") ?: return false
            if (!isRewritable(root, views, container.binLength)) return false

            val lossless = dataImageIndices(root)
            val replacements = HashMap<Int, Scaled>()
            for ((imageIndex, divisor) in plan.divisors) {
                val image = images.optJSONObject(imageIndex) ?: continue
                val viewIndex = image.optInt("bufferView", -1)
                if (viewIndex < 0 || replacements.containsKey(viewIndex)) continue
                val view = views.optJSONObject(viewIndex) ?: continue
                val length = view.optLong("byteLength", 0L)
                if (length <= 0L || length > MAX_IMAGE_BYTES) continue
                val encoded = ByteArray(length.toInt())
                input.seek(container.binOffset + view.optLong("byteOffset", 0L))
                input.readFully(encoded)
                val mimeType = sniff(encoded, encoded.size).mimeType ?: continue
                val scaled = scaler.scale(encoded, mimeType, divisor, imageIndex in lossless) ?: continue
                replacements[viewIndex] = scaled
            }
            if (replacements.isEmpty()) return false
            for (i in 0 until images.length()) {
                val image = images.optJSONObject(i) ?: continue
                val replacement = replacements[image.optInt("bufferView", -1)] ?: continue
                image.put("mimeType", replacement.mimeType)
            }

            // New layout: the views in their old order, each starting on a multiple of four.
            class Piece(val targetOffset: Long, val sourceOffset: Long, val length: Long, val data: ByteArray?)
            val order = (0 until views.length()).sortedWith(
                compareBy<Int>({ views.getJSONObject(it).optLong("byteOffset", 0L) }, { it })
            )
            val pieces = ArrayList<Piece>(order.size)
            val placed = HashMap<Pair<Long, Long>, Long>()
            var cursor = 0L
            for (viewIndex in order) {
                val view = views.getJSONObject(viewIndex)
                val span = Pair(view.optLong("byteOffset", 0L), view.optLong("byteLength", 0L))
                val replacement = replacements[viewIndex]
                if (replacement == null) {
                    val earlier = placed[span]
                    if (earlier != null) {
                        view.put("byteOffset", earlier)
                        continue
                    }
                }
                cursor = alignToFour(cursor)
                if (replacement != null) {
                    pieces += Piece(cursor, -1L, replacement.bytes.size.toLong(), replacement.bytes)
                    view.put("byteLength", replacement.bytes.size)
                    view.put("byteOffset", cursor)
                    cursor += replacement.bytes.size.toLong()
                } else {
                    pieces += Piece(cursor, span.first, span.second, null)
                    placed[span] = cursor
                    view.put("byteOffset", cursor)
                    cursor += span.second
                }
            }
            val binLength = alignToFour(cursor)
            root.getJSONArray("buffers").getJSONObject(0).put("byteLength", binLength)

            val jsonText = root.toString().toByteArray(Charsets.UTF_8)
            val jsonLength = alignToFour(jsonText.size.toLong())
            val total = 12L + 8L + jsonLength + 8L + binLength
            if (total > 0xFFFFFFFFL) return false

            BufferedOutputStream(FileOutputStream(target), 256 * 1024).use { output ->
                writeU32(output, MAGIC_GLTF)
                writeU32(output, 2L)
                writeU32(output, total)
                writeU32(output, jsonLength)
                writeU32(output, CHUNK_JSON)
                output.write(jsonText)
                repeat((jsonLength - jsonText.size).toInt()) { output.write(' '.code) }
                writeU32(output, binLength)
                writeU32(output, CHUNK_BIN)
                var written = 0L
                val buffer = ByteArray(256 * 1024)
                for (piece in pieces) {
                    while (written < piece.targetOffset) {
                        output.write(0)
                        written++
                    }
                    if (piece.data != null) {
                        output.write(piece.data)
                    } else {
                        input.seek(container.binOffset + piece.sourceOffset)
                        var remaining = piece.length
                        while (remaining > 0L) {
                            val count = input.read(buffer, 0, minOf(buffer.size.toLong(), remaining).toInt())
                            if (count < 0) return false
                            output.write(buffer, 0, count)
                            remaining -= count
                        }
                    }
                    written += piece.length
                }
                while (written < binLength) {
                    output.write(0)
                    written++
                }
            }
        }
        return true
    }

    /** What the first bytes of an embedded image say it is. Width and height are 0 when unreadable. */
    data class Sniffed(val mimeType: String?, val width: Int, val height: Int)

    fun sniff(bytes: ByteArray, length: Int): Sniffed {
        val n = minOf(length, bytes.size)
        fun u8(i: Int): Int = bytes[i].toInt() and 0xFF
        // PNG: signature, then the IHDR chunk with big-endian width and height.
        if (n >= 24 && u8(0) == 0x89 && u8(1) == 0x50 && u8(2) == 0x4E && u8(3) == 0x47 &&
            u8(12) == 0x49 && u8(13) == 0x48 && u8(14) == 0x44 && u8(15) == 0x52
        ) {
            val width = (u8(16) shl 24) or (u8(17) shl 16) or (u8(18) shl 8) or u8(19)
            val height = (u8(20) shl 24) or (u8(21) shl 16) or (u8(22) shl 8) or u8(23)
            return Sniffed(MIME_PNG, maxOf(0, width), maxOf(0, height))
        }
        // JPEG: walk the segments to the first start-of-frame marker.
        if (n >= 4 && u8(0) == 0xFF && u8(1) == 0xD8) {
            var i = 2
            while (i + 9 <= n) {
                if (u8(i) != 0xFF) {
                    i++
                    continue
                }
                val marker = u8(i + 1)
                if (marker == 0xFF) {
                    i++
                    continue
                }
                if (marker == 0xD8 || marker == 0x01 || marker in 0xD0..0xD7) {
                    i += 2
                    continue
                }
                val segment = (u8(i + 2) shl 8) or u8(i + 3)
                if (marker in 0xC0..0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
                    val height = (u8(i + 5) shl 8) or u8(i + 6)
                    val width = (u8(i + 7) shl 8) or u8(i + 8)
                    return Sniffed(MIME_JPEG, width, height)
                }
                if (segment < 2) break
                i += 2 + segment
            }
            return Sniffed(MIME_JPEG, 0, 0)
        }
        // WebP: RIFF container, three ways of storing the size.
        if (n >= 30 && u8(0) == 0x52 && u8(1) == 0x49 && u8(2) == 0x46 && u8(3) == 0x46 &&
            u8(8) == 0x57 && u8(9) == 0x45 && u8(10) == 0x42 && u8(11) == 0x50
        ) {
            val kind = String(bytes, 12, 4, Charsets.US_ASCII)
            return when (kind) {
                "VP8X" -> Sniffed(
                    MIME_WEBP,
                    1 + (u8(24) or (u8(25) shl 8) or (u8(26) shl 16)),
                    1 + (u8(27) or (u8(28) shl 8) or (u8(29) shl 16))
                )
                "VP8 " -> Sniffed(MIME_WEBP, (u8(26) or (u8(27) shl 8)) and 0x3FFF, (u8(28) or (u8(29) shl 8)) and 0x3FFF)
                "VP8L" -> {
                    val bits = u8(21).toLong() or (u8(22).toLong() shl 8) or (u8(23).toLong() shl 16) or (u8(24).toLong() shl 24)
                    Sniffed(MIME_WEBP, ((bits and 0x3FFFL) + 1L).toInt(), (((bits shr 14) and 0x3FFFL) + 1L).toInt())
                }
                else -> Sniffed(MIME_WEBP, 0, 0)
            }
        }
        return Sniffed(null, 0, 0)
    }

    private class Container(val json: JSONObject, val binOffset: Long, val binLength: Long)

    private fun isScalable(image: ImageInfo): Boolean =
        (image.mimeType == MIME_PNG || image.mimeType == MIME_JPEG) && image.width > 0 && image.height > 0

    private fun alignToFour(value: Long): Long = (value + 3L) and 3L.inv()

    private fun writeU32(output: OutputStream, value: Long) {
        output.write((value and 0xFF).toInt())
        output.write(((value shr 8) and 0xFF).toInt())
        output.write(((value shr 16) and 0xFF).toInt())
        output.write(((value shr 24) and 0xFF).toInt())
    }

    private fun littleEndian32(bytes: ByteArray, offset: Int): Long =
        (bytes[offset].toLong() and 0xFF) or
            ((bytes[offset + 1].toLong() and 0xFF) shl 8) or
            ((bytes[offset + 2].toLong() and 0xFF) shl 16) or
            ((bytes[offset + 3].toLong() and 0xFF) shl 24)

    private fun readContainer(source: RandomAccessFile): Container? {
        val fileLength = source.length()
        if (fileLength < 20L) return null
        val head = ByteArray(20)
        source.seek(0L)
        source.readFully(head)
        if (littleEndian32(head, 0) != MAGIC_GLTF || littleEndian32(head, 4) != 2L) return null
        val jsonLength = littleEndian32(head, 12)
        if (littleEndian32(head, 16) != CHUNK_JSON || jsonLength <= 0L || jsonLength > MAX_JSON_BYTES) return null
        if (20L + jsonLength > fileLength) return null
        val jsonBytes = ByteArray(jsonLength.toInt())
        source.readFully(jsonBytes)
        val root = try {
            JSONObject(String(jsonBytes, Charsets.UTF_8))
        } catch (_: Exception) {
            return null
        }
        var binOffset = 0L
        var binLength = 0L
        val next = 20L + jsonLength
        if (next + 8L <= fileLength) {
            val chunk = ByteArray(8)
            source.seek(next)
            source.readFully(chunk)
            val length = littleEndian32(chunk, 0)
            if (littleEndian32(chunk, 4) == CHUNK_BIN && next + 8L + length <= fileLength) {
                binOffset = next + 8L
                binLength = length
            }
        }
        return Container(root, binOffset, binLength)
    }

    private fun imagesOf(container: Container, source: RandomAccessFile): List<ImageInfo> {
        val images = container.json.optJSONArray("images") ?: return emptyList()
        val views = container.json.optJSONArray("bufferViews")
        val result = ArrayList<ImageInfo>(images.length())
        for (i in 0 until images.length()) {
            val image = images.optJSONObject(i)
            val viewIndex = image?.optInt("bufferView", -1) ?: -1
            val view = if (viewIndex >= 0) views?.optJSONObject(viewIndex) else null
            if (view == null) {
                result += ImageInfo(i, -1, null, 0, 0, 0L)
                continue
            }
            val offset = view.optLong("byteOffset", 0L)
            val length = view.optLong("byteLength", 0L)
            if (view.optInt("buffer", 0) != 0 || offset < 0L || length <= 0L || offset + length > container.binLength) {
                result += ImageInfo(i, viewIndex, null, 0, 0, maxOf(0L, length))
                continue
            }
            var probe = ByteArray(minOf(length, HEADER_PROBE_BYTES.toLong()).toInt())
            source.seek(container.binOffset + offset)
            source.readFully(probe)
            var sniffed = sniff(probe, probe.size)
            // A JPEG can carry metadata and a thumbnail ahead of its frame header: look further once.
            if (sniffed.mimeType == MIME_JPEG && sniffed.width == 0 && length > probe.size) {
                probe = ByteArray(minOf(length, LONG_HEADER_PROBE_BYTES.toLong()).toInt())
                source.seek(container.binOffset + offset)
                source.readFully(probe)
                sniffed = sniff(probe, probe.size)
            }
            result += ImageInfo(i, viewIndex, sniffed.mimeType, sniffed.width, sniffed.height, length)
        }
        return result
    }

    /** Images used as normal, occlusion or metallic-roughness maps: data, not colour. */
    private fun dataImageIndices(root: JSONObject): Set<Int> {
        val textures = root.optJSONArray("textures") ?: return emptySet()
        val materials = root.optJSONArray("materials") ?: return emptySet()
        val result = HashSet<Int>()
        fun add(reference: JSONObject?) {
            val texture = reference?.optInt("index", -1) ?: -1
            val source = if (texture >= 0) textures.optJSONObject(texture)?.optInt("source", -1) ?: -1 else -1
            if (source >= 0) result += source
        }
        for (m in 0 until materials.length()) {
            val material = materials.optJSONObject(m) ?: continue
            add(material.optJSONObject("normalTexture"))
            add(material.optJSONObject("occlusionTexture"))
            add(material.optJSONObject("pbrMetallicRoughness")?.optJSONObject("metallicRoughnessTexture"))
        }
        return result
    }

    private fun isRewritable(root: JSONObject, views: JSONArray, binLength: Long): Boolean {
        val buffers = root.optJSONArray("buffers") ?: return false
        if (buffers.length() != 1 || buffers.optJSONObject(0)?.has("uri") != false) return false
        val spans = ArrayList<Pair<Long, Long>>(views.length())
        for (i in 0 until views.length()) {
            val view = views.optJSONObject(i) ?: return false
            if (view.optInt("buffer", 0) != 0) return false
            if (view.optJSONObject("extensions")?.has("EXT_meshopt_compression") == true) return false
            val offset = view.optLong("byteOffset", 0L)
            val length = view.optLong("byteLength", 0L)
            if (offset < 0L || length < 0L || offset + length > binLength) return false
            spans += Pair(offset, length)
        }
        var end = 0L
        for (span in spans.distinct().sortedWith(compareBy<Pair<Long, Long>>({ it.first }, { it.second }))) {
            if (span.first < end) return false
            end = span.first + span.second
        }
        return true
    }
}
