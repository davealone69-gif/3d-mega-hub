package com.davemega.hub.avatarforge

/**
 * Computes the wall-clock budget for the initial 3 presented EGL frames in [AvatarForgeSurfaceView].
 *
 * Hardware GPUs (such as Samsung Xclipse 940 / Adreno / Mali) keep the strict 15-second gate.
 * Software rasterisers used on headless CI emulators (SwiftShader / llvmpipe / ANGLE SwiftShader)
 * render dense 1.6M-triangle generated avatars at ~6–10 seconds per frame, so they receive a
 * bounded 90-second budget that still fits inside instrumentation test timeouts.
 */
internal object RendererFrameBudget {
    const val HARDWARE_BUDGET_NANOS: Long = 15_000_000_000L
    const val SOFTWARE_BUDGET_NANOS: Long = 90_000_000_000L

    private val SOFTWARE_TOKENS = listOf(
        "swiftshader",
        "llvmpipe",
        "softpipe",
        "lavapipe",
        "android emulator",
        "emulated",
        "subzero"
    )

    fun isSoftwareRenderer(rendererInfo: String): Boolean {
        val normalized = rendererInfo.lowercase()
        return SOFTWARE_TOKENS.any { token -> normalized.contains(token) }
    }

    fun firstPresentBudgetNanos(rendererInfo: String): Long =
        if (isSoftwareRenderer(rendererInfo)) SOFTWARE_BUDGET_NANOS else HARDWARE_BUDGET_NANOS
}
