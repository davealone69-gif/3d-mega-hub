package com.davemega.hub

import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class GlbTextureBudgetTest {
    @get:Rule
    val folder = TemporaryFolder()

    private val megabyte = 1024L * 1024L

    // ---- image headers: just enough of each format for its size to be read ----

    private fun png(width: Int, height: Int, size: Int = 96): ByteArray {
        val bytes = ByteArray(maxOf(size, 40)) { (it * 31 + 7).toByte() }
        byteArrayOf(0x89.toByte(), 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A).copyInto(bytes)
        byteArrayOf(0, 0, 0, 13).copyInto(bytes, 8)
        "IHDR".toByteArray(Charsets.US_ASCII).copyInto(bytes, 12)
        ByteBuffer.wrap(bytes, 16, 8).order(ByteOrder.BIG_ENDIAN).putInt(width).putInt(height)
        return bytes
    }

    private fun jpeg(width: Int, height: Int, size: Int = 96): ByteArray {
        val out = ByteArrayOutputStream()
        out.write(byteArrayOf(0xFF.toByte(), 0xD8.toByte()))
        out.write(byteArrayOf(0xFF.toByte(), 0xE0.toByte(), 0, 16))
        out.write(ByteArray(14))
        out.write(byteArrayOf(0xFF.toByte(), 0xC0.toByte(), 0, 17, 8))
        out.write(byteArrayOf((height shr 8).toByte(), height.toByte(), (width shr 8).toByte(), width.toByte()))
        while (out.size() < size) out.write(out.size() * 17)
        return out.toByteArray()
    }

    private fun webp(width: Int, height: Int): ByteArray {
        val bytes = ByteArray(64)
        "RIFF".toByteArray(Charsets.US_ASCII).copyInto(bytes, 0)
        "WEBP".toByteArray(Charsets.US_ASCII).copyInto(bytes, 8)
        "VP8X".toByteArray(Charsets.US_ASCII).copyInto(bytes, 12)
        bytes[16] = 10
        val w = width - 1
        val h = height - 1
        bytes[24] = w.toByte(); bytes[25] = (w shr 8).toByte(); bytes[26] = (w shr 16).toByte()
        bytes[27] = h.toByte(); bytes[28] = (h shr 8).toByte(); bytes[29] = (h shr 16).toByte()
        return bytes
    }

    // ---- a GLB made of buffer views; some of them are images ----

    private class Part(val bytes: ByteArray, val mimeType: String? = null) {
        val isImage: Boolean get() = mimeType != null
    }

    private fun glb(parts: List<Part>, generator: String = "unit test", edit: (JSONObject) -> Unit = {}): ByteArray {
        val bin = ByteArrayOutputStream()
        val views = JSONArray()
        val images = JSONArray()
        for (part in parts) {
            while (bin.size() % 4 != 0) bin.write(0)
            views.put(JSONObject().put("buffer", 0).put("byteOffset", bin.size()).put("byteLength", part.bytes.size))
            if (part.isImage) images.put(JSONObject().put("bufferView", views.length() - 1).put("mimeType", part.mimeType))
            bin.write(part.bytes)
        }
        while (bin.size() % 4 != 0) bin.write(0)
        val root = JSONObject()
            .put("asset", JSONObject().put("version", "2.0").put("generator", generator))
            .put("buffers", JSONArray().put(JSONObject().put("byteLength", bin.size())))
            .put("bufferViews", views)
            .put("images", images)
        edit(root)
        val json = root.toString().toByteArray(Charsets.UTF_8)
        val pad = (4 - json.size % 4) % 4
        val total = 12 + 8 + json.size + pad + 8 + bin.size()
        val out = ByteBuffer.allocate(total).order(ByteOrder.LITTLE_ENDIAN)
        out.putInt(0x46546C67).putInt(2).putInt(total)
        out.putInt(json.size + pad).putInt(0x4E4F534A).put(json)
        repeat(pad) { out.put(' '.code.toByte()) }
        out.putInt(bin.size()).putInt(0x004E4942).put(bin.toByteArray())
        return out.array()
    }

    private fun write(name: String, bytes: ByteArray): File = folder.newFile(name).also { it.writeBytes(bytes) }

    private class Parsed(val total: Int, val json: JSONObject, val bin: ByteArray) {
        fun view(index: Int): ByteArray {
            val view = json.getJSONArray("bufferViews").getJSONObject(index)
            val offset = view.optInt("byteOffset", 0)
            return bin.copyOfRange(offset, offset + view.getInt("byteLength"))
        }
    }

    private fun parse(file: File): Parsed {
        val buffer = ByteBuffer.wrap(file.readBytes()).order(ByteOrder.LITTLE_ENDIAN)
        assertEquals(0x46546C67, buffer.int)
        assertEquals(2, buffer.int)
        val total = buffer.int
        val jsonLength = buffer.int
        assertEquals(0x4E4F534A, buffer.int)
        val json = ByteArray(jsonLength).also { buffer.get(it) }
        val binLength = buffer.int
        assertEquals(0x004E4942, buffer.int)
        val bin = ByteArray(binLength).also { buffer.get(it) }
        assertEquals("nothing may follow the binary chunk", 0, buffer.remaining())
        return Parsed(total, JSONObject(String(json, Charsets.UTF_8)), bin)
    }

    /** Stands in for the Android bitmap code: returns bytes that say what it was asked to do. */
    private class RecordingScaler(
        private val refuse: Boolean = false,
        private val opaquePngAsJpeg: Boolean = false
    ) : GlbTextureBudget.Scaler {
        val calls = ArrayList<String>()
        override fun scale(encoded: ByteArray, mimeType: String, divisor: Int, lossless: Boolean): GlbTextureBudget.Scaled? {
            calls += "$mimeType/$divisor/$lossless/${encoded.size}"
            if (refuse) return null
            val keepPng = lossless || (mimeType == GlbTextureBudget.MIME_PNG && !opaquePngAsJpeg)
            val out = if (keepPng) GlbTextureBudget.MIME_PNG else GlbTextureBudget.MIME_JPEG
            // An odd length, so the view after it has to be re-aligned.
            return GlbTextureBudget.Scaled("scaled:$mimeType:$divisor".toByteArray(Charsets.US_ASCII) + byteArrayOf(1), out)
        }
    }

    @Test
    fun sizesAreReadFromPngJpegAndWebpHeaders() {
        val a = png(4096, 2048)
        assertEquals(GlbTextureBudget.Sniffed(GlbTextureBudget.MIME_PNG, 4096, 2048), GlbTextureBudget.sniff(a, a.size))
        val b = jpeg(1024, 512)
        assertEquals(GlbTextureBudget.Sniffed(GlbTextureBudget.MIME_JPEG, 1024, 512), GlbTextureBudget.sniff(b, b.size))
        val c = webp(2048, 1024)
        assertEquals(GlbTextureBudget.Sniffed(GlbTextureBudget.MIME_WEBP, 2048, 1024), GlbTextureBudget.sniff(c, c.size))
        val d = ByteArray(64) { 3 }
        assertEquals(GlbTextureBudget.Sniffed(null, 0, 0), GlbTextureBudget.sniff(d, d.size))
        // A truncated header must not be read past its end.
        assertEquals(GlbTextureBudget.Sniffed(null, 0, 0), GlbTextureBudget.sniff(a, 12))
        assertEquals(GlbTextureBudget.Sniffed(GlbTextureBudget.MIME_JPEG, 0, 0), GlbTextureBudget.sniff(b, 8))
    }

    @Test
    fun decodedSizeCountsTheMipChain() {
        // 64 MiB for the top level and a third of that again for the levels below it.
        assertEquals(85L, GlbTextureBudget.decodedBytes(4096, 4096) / megabyte)
        assertEquals(21L, GlbTextureBudget.decodedBytes(2048, 2048) / megabyte)
        assertTrue(GlbTextureBudget.decodedBytes(4096, 4096) > 4096L * 4096L * 4L)
        assertEquals(0L, GlbTextureBudget.decodedBytes(0, 100))
    }

    @Test
    fun imagesOfAFileAreListedWithTheirSizes() {
        val file = write(
            "three.glb",
            glb(listOf(Part(ByteArray(48) { 9 }), Part(png(4096, 4096), "image/png"), Part(jpeg(1024, 512), "image/jpeg"), Part(webp(256, 256), "image/webp")))
        )
        val images = GlbTextureBudget.inspect(file)
        assertNotNull(images)
        assertEquals(3, images!!.size)
        assertEquals(listOf(4096, 1024, 256), images.map { it.width })
        assertEquals(listOf(4096, 512, 256), images.map { it.height })
        assertEquals(listOf("image/png", "image/jpeg", "image/webp"), images.map { it.mimeType })
        assertEquals(listOf(1, 2, 3), images.map { it.bufferView })
        assertNull(GlbTextureBudget.inspect(write("junk.glb", ByteArray(64) { 1 })))
    }

    @Test
    fun aModelInsideTheBudgetIsLeftAlone() {
        val images = (0 until 25).map { GlbTextureBudget.ImageInfo(it, it, "image/png", 1024, 1024, 1000L) }
        val plan = GlbTextureBudget.plan(images)
        assertFalse(plan.needsRewrite)
        assertTrue(plan.fitsBudget)
        assertEquals(plan.decodedBefore, plan.decodedAfter)
    }

    @Test
    fun theOwnersDazModelIsHalvedOnceAndFits() {
        // "Ink": twelve 4096 x 4096 and six 2048 x 2048 textures, 1,152 MiB decoded.
        val images = (0 until 18).map {
            val side = if (it < 12) 4096 else 2048
            GlbTextureBudget.ImageInfo(it, it, if (it % 2 == 0) "image/png" else "image/jpeg", side, side, 1000L)
        }
        val plan = GlbTextureBudget.plan(images)
        assertEquals(1152L, plan.decodedBefore / megabyte)
        assertTrue(plan.needsRewrite)
        assertEquals(2048, plan.sideLimit)
        assertEquals((0 until 12).toSet(), plan.divisors.keys)
        assertTrue(plan.divisors.values.all { it == 2 })
        assertEquals(384L, plan.decodedAfter / megabyte)
        assertTrue(plan.fitsBudget)
        assertTrue(GlbTextureBudget.describe(plan).contains("12 of 18"))
    }

    @Test
    fun theLimitDropsUntilTheSumFits() {
        val images = listOf(GlbTextureBudget.ImageInfo(0, 0, "image/png", 8192, 8192, 1000L))
        val plan = GlbTextureBudget.plan(images, 8L * megabyte)
        assertEquals(1024, plan.sideLimit)
        assertEquals(mapOf(0 to 8), plan.divisors)
        assertTrue(plan.fitsBudget)
    }

    @Test
    fun formatsThatCannotBeRewrittenAreCountedButNotPlanned() {
        val images = listOf(GlbTextureBudget.ImageInfo(0, 0, "image/webp", 4096, 4096, 1000L))
        val plan = GlbTextureBudget.plan(images, 10L * megabyte)
        assertFalse(plan.needsRewrite)
        assertFalse(plan.fitsBudget)
    }

    @Test
    fun rewriteReplacesOnlyThePlannedImagesAndKeepsEveryOtherByte() {
        val geometryA = ByteArray(52) { (it + 1).toByte() }
        val geometryB = ByteArray(30) { (100 - it).toByte() }
        val small = jpeg(1024, 1024)
        val source = write(
            "model.glb",
            glb(listOf(Part(geometryA), Part(png(4096, 4096, 200), "image/png"), Part(geometryB), Part(small, "image/jpeg")))
        )
        val plan = GlbTextureBudget.plan(GlbTextureBudget.inspect(source)!!, 32L * megabyte)
        assertEquals(mapOf(0 to 2), plan.divisors)

        val target = File(folder.root, "model.fit.glb")
        val scaler = RecordingScaler()
        assertTrue(GlbTextureBudget.rewrite(source, target, plan, scaler))
        assertEquals(listOf("image/png/2/false/200"), scaler.calls)

        val parsed = parse(target)
        assertEquals(target.length().toInt(), parsed.total)
        assertEquals(parsed.bin.size, parsed.json.getJSONArray("buffers").getJSONObject(0).getInt("byteLength"))
        assertEquals(0, parsed.bin.size % 4)
        val views = parsed.json.getJSONArray("bufferViews")
        for (i in 0 until views.length()) assertEquals("view $i must start on a multiple of four", 0, views.getJSONObject(i).getInt("byteOffset") % 4)
        assertArrayEquals(geometryA, parsed.view(0))
        assertArrayEquals(geometryB, parsed.view(2))
        assertArrayEquals(small, parsed.view(3))
        assertEquals("scaled:image/png:2", String(parsed.view(1), Charsets.US_ASCII).dropLast(1))
        assertEquals("image/png", parsed.json.getJSONArray("images").getJSONObject(0).getString("mimeType"))
        assertEquals("image/jpeg", parsed.json.getJSONArray("images").getJSONObject(1).getString("mimeType"))
        assertEquals("unit test", parsed.json.getJSONObject("asset").getString("generator"))
        // The rewritten file is a GLB the app's own validator accepts.
        com.davemega.hub.avatarforge.GlbValidator.validate(target)
    }

    @Test
    fun theMimeTypeFollowsWhatTheScalerWrote() {
        // The Android scaler writes an opaque PNG as a JPEG; the file has to say so.
        val source = write("opaque.glb", glb(listOf(Part(png(4096, 4096, 300), "image/png"), Part(ByteArray(16) { 5 }))))
        val plan = GlbTextureBudget.plan(GlbTextureBudget.inspect(source)!!, 32L * megabyte)
        val target = File(folder.root, "opaque.fit.glb")
        assertTrue(GlbTextureBudget.rewrite(source, target, plan, RecordingScaler(opaquePngAsJpeg = true)))
        val parsed = parse(target)
        assertEquals("image/jpeg", parsed.json.getJSONArray("images").getJSONObject(0).getString("mimeType"))
        assertArrayEquals(ByteArray(16) { 5 }, parsed.view(1))
    }

    @Test
    fun normalMapsAreScaledWithoutLoss() {
        val source = write(
            "normal.glb",
            glb(listOf(Part(png(4096, 4096), "image/png"), Part(jpeg(4096, 4096), "image/jpeg"))) { root ->
                root.put("textures", JSONArray().put(JSONObject().put("source", 0)).put(JSONObject().put("source", 1)))
                root.put(
                    "materials",
                    JSONArray().put(
                        JSONObject()
                            .put("pbrMetallicRoughness", JSONObject().put("baseColorTexture", JSONObject().put("index", 0)))
                            .put("normalTexture", JSONObject().put("index", 1))
                    )
                )
            }
        )
        val plan = GlbTextureBudget.plan(GlbTextureBudget.inspect(source)!!, 64L * megabyte)
        assertEquals(setOf(0, 1), plan.divisors.keys)
        val scaler = RecordingScaler()
        assertTrue(GlbTextureBudget.rewrite(source, File(folder.root, "normal.fit.glb"), plan, scaler))
        assertEquals(1, scaler.calls.count { it.startsWith("image/png/2/false/") })
        assertEquals(1, scaler.calls.count { it.startsWith("image/jpeg/2/true/") })
    }

    @Test
    fun layoutsThatAreNotUnderstoodAreNotRewritten() {
        val parts = listOf(Part(png(4096, 4096), "image/png"), Part(ByteArray(16) { 5 }))
        val twoBuffers = write("two.glb", glb(parts) { it.getJSONArray("buffers").put(JSONObject().put("byteLength", 4)) })
        val external = write("uri.glb", glb(parts) { it.getJSONArray("buffers").getJSONObject(0).put("uri", "data.bin") })
        val meshopt = write(
            "meshopt.glb",
            glb(parts) {
                it.getJSONArray("bufferViews").getJSONObject(1)
                    .put("extensions", JSONObject().put("EXT_meshopt_compression", JSONObject()))
            }
        )
        val overlapping = write("overlap.glb", glb(parts) { it.getJSONArray("bufferViews").getJSONObject(1).put("byteOffset", 4) })
        for (file in listOf(twoBuffers, external, meshopt, overlapping)) {
            val plan = GlbTextureBudget.plan(GlbTextureBudget.inspect(file)!!, 32L * megabyte)
            assertTrue(file.name, plan.needsRewrite)
            assertFalse(file.name, GlbTextureBudget.rewrite(file, File(folder.root, file.name + ".out"), plan, RecordingScaler()))
        }
    }

    @Test
    fun aRefusingScalerLeavesNoRewrite() {
        val source = write("refused.glb", glb(listOf(Part(png(4096, 4096), "image/png"))))
        val plan = GlbTextureBudget.plan(GlbTextureBudget.inspect(source)!!, 32L * megabyte)
        assertFalse(GlbTextureBudget.rewrite(source, File(folder.root, "refused.out"), plan, RecordingScaler(refuse = true)))
    }

    // ---- the import step that uses it ----

    @Test
    fun importKeepsASmallModelAsItIs() {
        val source = write("small.glb", glb(listOf(Part(png(1024, 1024), "image/png"))))
        val result = ImportedGlbPreparation.fitTextures(source, RecordingScaler())
        assertSame(source, result.file)
        assertNull(result.note)
        assertTrue(source.isFile)
    }

    @Test
    fun importNeverRewritesAnAvatarThisAppMade() {
        val source = write("ours.glb", glb(listOf(Part(png(4096, 4096), "image/png")), generator = "AvatarForge 0.10.1"))
        val scaler = RecordingScaler()
        val result = ImportedGlbPreparation.fitTextures(source, scaler, budgetBytes = 8L * megabyte)
        assertSame(source, result.file)
        assertNull(result.note)
        assertTrue(scaler.calls.isEmpty())
    }

    @Test
    fun importReplacesAnOversizedModelByAReducedCopy() {
        val geometry = ByteArray(40) { (it * 3).toByte() }
        val source = write("big.glb", glb(listOf(Part(geometry), Part(png(4096, 4096), "image/png"), Part(jpeg(4096, 4096), "image/jpeg"))))
        val progress = ArrayList<String>()
        val result = ImportedGlbPreparation.fitTextures(source, RecordingScaler(), budgetBytes = 64L * megabyte) { progress += it }
        assertFalse("the oversized copy must be gone", source.exists())
        assertTrue(result.file.isFile)
        assertNotNull(result.note)
        assertTrue(result.note!!.contains("2 of 2 textures reduced"))
        assertEquals(1, progress.size)
        assertArrayEquals(geometry, parse(result.file).view(0))
    }

    @Test
    fun importKeepsTheOriginalWhenNothingCouldBeReduced() {
        val source = write("stuck.glb", glb(listOf(Part(png(4096, 4096), "image/png"))))
        val result = ImportedGlbPreparation.fitTextures(source, RecordingScaler(refuse = true), budgetBytes = 8L * megabyte)
        assertSame(source, result.file)
        assertTrue(source.isFile)
        assertTrue(result.note!!.contains("could not be reduced"))
        assertFalse(File(source.parentFile, source.name + ".fit").exists())
    }
}
