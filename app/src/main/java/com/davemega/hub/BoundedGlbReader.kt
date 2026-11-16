package com.davemega.hub

import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.EOFException
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream

/**
 * Reads a GLB through a bounded disk spool, then allocates one exact-sized byte array.
 *
 * ByteArrayOutputStream.toByteArray() duplicates its backing buffer. Near the 128 MiB import
 * ceiling that creates a second full-size allocation while the first buffer is still live,
 * which can exhaust an Android app heap before Filament has even parsed the model.
 */
internal object BoundedGlbReader {
    fun read(input: InputStream, cacheDirectory: File, maxBytes: Long): ByteArray =
        input.use { source ->
            require(maxBytes in 1L..Int.MAX_VALUE.toLong()) { "GLB safety limit is invalid: $maxBytes bytes" }
            require(cacheDirectory.isDirectory || cacheDirectory.mkdirs()) {
                "GLB cache directory is unavailable: ${cacheDirectory.absolutePath}"
            }
            val spool = File.createTempFile("mega-hub-glb-", ".tmp", cacheDirectory)
            try {
                var total = 0L
                BufferedOutputStream(FileOutputStream(spool)).use { output ->
                    val buffer = ByteArray(64 * 1024)
                    while (true) {
                        val count = source.read(buffer)
                        if (count < 0) break
                        if (count == 0) {
                            val singleByte = source.read()
                            if (singleByte < 0) break
                            total += 1L
                            require(total <= maxBytes) {
                                "GLB exceeds the viewer's ${maxBytes / (1024L * 1024L)} MiB safety limit"
                            }
                            output.write(singleByte)
                            continue
                        }
                        total += count
                        require(total <= maxBytes) {
                            "GLB exceeds the viewer's ${maxBytes / (1024L * 1024L)} MiB safety limit"
                        }
                        output.write(buffer, 0, count)
                    }
                }

                require(total > 0L) { "Selected GLB is empty" }
                check(spool.length() == total) {
                    "Temporary GLB size changed while reading: expected $total bytes, found ${spool.length()}"
                }

                val result = ByteArray(total.toInt())
                BufferedInputStream(FileInputStream(spool)).use { stored ->
                    var offset = 0
                    while (offset < result.size) {
                        val count = stored.read(result, offset, result.size - offset)
                        if (count < 0) throw EOFException("Temporary GLB ended after $offset of ${result.size} bytes")
                        if (count == 0) continue
                        offset += count
                    }
                    check(stored.read() == -1) { "Temporary GLB grew while being read" }
                }
                result
            } finally {
                if (spool.exists() && !spool.delete()) spool.deleteOnExit()
            }
        }
    }
