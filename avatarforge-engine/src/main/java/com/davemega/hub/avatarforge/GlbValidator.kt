package com.davemega.hub.avatarforge

import java.io.BufferedInputStream
import java.io.File
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.CodingErrorAction
import java.nio.charset.StandardCharsets

/**
 * Bounded structural validation for generated glTF Binary (GLB) files.
 * This catches truncated or malformed native output before it enters the avatar library.
 */
object GlbValidator {
    private const val GLB_MAGIC = 0x46546C67L
    private const val JSON_CHUNK = 0x4E4F534AL
    private const val MAX_JSON_CHUNK_BYTES = 16L * 1024L * 1024L
    const val MAX_GLB_BYTES = 128L * 1024L * 1024L

    fun validate(file: File) {
        require(file.isFile) { "Generated GLB file is missing." }
        val fileLength = file.length()
        require(fileLength in 20L..MAX_GLB_BYTES) {
            "Generated GLB size is invalid: $fileLength bytes."
        }

        BufferedInputStream(file.inputStream()).use { input ->
            require(readU32(input) == GLB_MAGIC) { "Generated file does not have the GLB magic header." }
            require(readU32(input) == 2L) { "Generated GLB must use glTF binary version 2." }
            require(readU32(input) == fileLength) { "Generated GLB header length does not match the file size." }

            var offset = 12L
            var chunkIndex = 0
            var jsonFound = false
            while (offset < fileLength) {
                require(fileLength - offset >= 8L) { "Generated GLB ends inside a chunk header." }
                val chunkLength = readU32(input)
                val chunkType = readU32(input)
                offset += 8L

                require(chunkLength % 4L == 0L) { "Generated GLB contains a misaligned chunk." }
                require(chunkLength <= fileLength - offset) { "Generated GLB contains a truncated chunk." }

                if (chunkIndex == 0) {
                    require(chunkType == JSON_CHUNK) { "The first GLB chunk must contain JSON." }
                    require(chunkLength in 2L..MAX_JSON_CHUNK_BYTES) { "Generated GLB JSON chunk size is invalid." }
                    val jsonBytes = ByteArray(chunkLength.toInt())
                    input.readFully(jsonBytes)
                    val json = StandardCharsets.UTF_8.newDecoder()
                        .onMalformedInput(CodingErrorAction.REPORT)
                        .onUnmappableCharacter(CodingErrorAction.REPORT)
                        .decode(ByteBuffer.wrap(jsonBytes))
                        .toString()
                        .trimEnd(' ', '\t', '\r', '\n')
                    require(json.startsWith("{") && json.endsWith("}")) {
                        "Generated GLB JSON chunk is not a JSON object."
                    }
                    require(Regex("\"asset\"\\s*:\\s*\\{[^}]*\"version\"\\s*:\\s*\"2\\.0\"")
                        .containsMatchIn(json)) {
                        "Generated GLB JSON does not declare glTF asset version 2.0."
                    }
                    jsonFound = true
                } else {
                    skipFully(input, chunkLength)
                }

                offset += chunkLength
                chunkIndex++
            }

            require(jsonFound && chunkIndex > 0) { "Generated GLB has no JSON chunk." }
            require(offset == fileLength) { "Generated GLB contains trailing or unparsed bytes." }
        }
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
            if (skipped > 0L) {
                remaining -= skipped
            } else {
                require(input.read() != -1) { "Generated GLB ended before a chunk was complete." }
                remaining--
            }
        }
    }

    private fun InputStream.readFully(bytes: ByteArray) {
        var offset = 0
        while (offset < bytes.size) {
            val count = read(bytes, offset, bytes.size - offset)
            require(count > 0) { "Generated GLB ended unexpectedly." }
            offset += count
        }
    }
}
