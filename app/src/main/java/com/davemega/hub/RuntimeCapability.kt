package com.davemega.hub

/** Missing or unhealthy backends are never presented as ready or device-verified. */
data class RuntimeCapability(
    val name: String,
    val backend: String? = null,
    val initialized: Boolean = false,
    val problem: String? = null,
    val detail: String? = null,
) {
    val status: String
        get() = when {
            backend.isNullOrBlank() -> "NOT INTEGRATED"
            problem != null -> "UNAVAILABLE"
            !initialized -> "UNVERIFIED"
            else -> "INITIALIZED, NOT DEVICE-VERIFIED"
        }
}
