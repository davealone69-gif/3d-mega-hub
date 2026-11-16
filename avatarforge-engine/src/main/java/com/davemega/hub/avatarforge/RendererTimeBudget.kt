package com.davemega.hub.avatarforge

/**
 * How long the GLES renderer waits for a presentable frame before reporting a failure.
 *
 * The original budget was a flat 15 s, which is generous on hardware and impossible on a software
 * rasteriser: the exact-head Android CI runs on an emulator whose SwiftShader path was asked to
 * present three swaps of a 1,626,176-triangle GLB, and reached only two in 15 s. The renderer is
 * healthy there; the budget was measuring the emulator's CPU.
 *
 * This keeps the hardware budget strict and widens it only when the GL or OS strings identify a
 * software renderer or an emulator. The assertions themselves - presented frames, draw calls,
 * triangles, in-frustum bounds, verified pixels - are unchanged.
 */
object RendererTimeBudget {
    const val HARDWARE_FRAME_WAIT_MS = 15_000L
    const val SOFTWARE_FRAME_WAIT_MS = 180_000L

    private val SOFTWARE_MARKERS = listOf(
        "swiftshader",
        "emulator",
        "goldfish",
        "ranchu",
        "sdk_gphone",
        "emu64",
        "lavapipe",
        "llvmpipe"
    )

    fun frameWaitMillis(apiVersion: String, fingerprint: String, model: String = ""): Long =
        if (isSoftwareRenderer(apiVersion, fingerprint, model)) SOFTWARE_FRAME_WAIT_MS
        else HARDWARE_FRAME_WAIT_MS

    fun isSoftwareRenderer(apiVersion: String, fingerprint: String, model: String = ""): Boolean =
        listOf(apiVersion, fingerprint, model).any { candidate ->
            SOFTWARE_MARKERS.any { maker -> candidate.contains(maker, ignoreCase = true) }
        }
}
