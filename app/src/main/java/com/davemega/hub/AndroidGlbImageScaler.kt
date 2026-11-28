package com.davemega.hub

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import java.io.ByteArrayOutputStream

/**
 * Halves, quarters or further reduces one embedded PNG or JPEG with the platform's decoder.
 *
 * `inSampleSize` makes the decoder produce the smaller image directly, so a 4096 x 4096 source is
 * never held at full size. Alpha is kept straight (not premultiplied), so the colour of a fully
 * transparent texel survives: hair cards and lashes would otherwise get dark fringes. An image with
 * transparency, and any image used as a normal, occlusion or metallic-roughness map, is written as
 * PNG; an opaque colour image is written as JPEG.
 */
internal object AndroidGlbImageScaler : GlbTextureBudget.Scaler {
    private const val JPEG_QUALITY = 92

    override fun scale(encoded: ByteArray, mimeType: String, divisor: Int, lossless: Boolean): GlbTextureBudget.Scaled? {
        if (mimeType != GlbTextureBudget.MIME_PNG && mimeType != GlbTextureBudget.MIME_JPEG) return null
        if (divisor < 2) return null
        val options = BitmapFactory.Options()
        options.inSampleSize = divisor
        options.inPremultiplied = false
        options.inScaled = false
        options.inPreferredConfig = Bitmap.Config.ARGB_8888
        val bitmap: Bitmap = try {
            BitmapFactory.decodeByteArray(encoded, 0, encoded.size, options)
        } catch (_: OutOfMemoryError) {
            null
        } catch (_: RuntimeException) {
            null
        } ?: return null
        try {
            val asPng = lossless || (mimeType == GlbTextureBudget.MIME_PNG && bitmap.hasAlpha())
            val output = ByteArrayOutputStream(maxOf(64 * 1024, encoded.size / (divisor * divisor)))
            val written = if (asPng) {
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, output)
            } else {
                bitmap.compress(Bitmap.CompressFormat.JPEG, JPEG_QUALITY, output)
            }
            if (!written) return null
            return GlbTextureBudget.Scaled(
                output.toByteArray(),
                if (asPng) GlbTextureBudget.MIME_PNG else GlbTextureBudget.MIME_JPEG
            )
        } catch (_: OutOfMemoryError) {
            return null
        } finally {
            bitmap.recycle()
        }
    }
}
