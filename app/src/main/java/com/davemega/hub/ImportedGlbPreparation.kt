package com.davemega.hub

import com.davemega.hub.avatarforge.GlbValidator
import java.io.File

/**
 * The step between "the file was copied and is a valid GLB" and "the file is the current avatar":
 * a model whose textures would not fit in memory is replaced by a copy with smaller ones.
 *
 * The copy is what the app keeps and what both previews open, so the cost is paid once, at import.
 * The file the user picked is not touched. An avatar this app generated is never rewritten.
 */
internal object ImportedGlbPreparation {
    /** [file] is the file to keep; [note] says what was done, in words for the status line. */
    class Result(val file: File, val note: String?)

    fun fitTextures(
        source: File,
        scaler: GlbTextureBudget.Scaler,
        budgetBytes: Long = GlbTextureBudget.MAX_DECODED_TEXTURE_BYTES,
        onProgress: (String) -> Unit = {}
    ): Result {
        if (GlbPreviewRouting.isAvatarForgeExport(GlbPreviewRouting.summarize(source))) return Result(source, null)
        val images = GlbTextureBudget.inspect(source) ?: return Result(source, null)
        val plan = GlbTextureBudget.plan(images, budgetBytes)
        if (!plan.needsRewrite) return Result(source, null)

        val megabyte = 1024L * 1024L
        onProgress("Reducing ${plan.divisors.size} textures: they need ${plan.decodedBefore / megabyte} MB decoded…")
        val target = File(source.parentFile, source.name + ".fit")
        val rewritten = try {
            GlbTextureBudget.rewrite(source, target, plan, scaler) && target.isFile && run {
                GlbValidator.validate(target)
                true
            }
        } catch (_: Exception) {
            false
        } catch (_: OutOfMemoryError) {
            false
        }
        if (!rewritten) {
            target.delete()
            return Result(
                source,
                "its textures need ${plan.decodedBefore / megabyte} MB decoded and could not be reduced"
            )
        }
        // Report what the rewritten file really holds, not what was planned.
        val after = GlbTextureBudget.inspect(target)?.sumOf { it.decodedBytes } ?: plan.decodedAfter
        source.delete()
        return Result(
            target,
            "${plan.divisors.size} of ${plan.images.size} textures reduced to at most ${plan.sideLimit} pixels a side " +
                "(${plan.decodedBefore / megabyte} MB decoded before, ${after / megabyte} MB after)"
        )
    }
}
