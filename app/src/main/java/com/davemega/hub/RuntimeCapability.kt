package com.davemega.hub

/** Missing backends are never presented as ready or verified. */
data class RuntimeCapability(
    val name: String,
    val backend: String? = null,
    val initialized: Boolean = false
) {
    val status: String
        get() = when {
            backend.isNullOrBlank() -> "NOT INTEGRATED"
            !initialized -> "UNVERIFIED"
            else -> "INITIALIZED, NOT DEVICE-VERIFIED"
        }
}
