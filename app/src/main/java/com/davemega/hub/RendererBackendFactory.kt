package com.davemega.hub

import com.google.android.filament.Engine

internal data class RendererBackendSelection<T : Any>(
    val backend: Engine.Backend,
    val value: T
)

/**
 * Prefer the known GLES path on the target Samsung device. Vulkan is an optional
 * fallback and is not treated as available until the complete session initializes.
 */
internal fun <T : Any> createWithRendererBackendFallback(
    initialize: (Engine.Backend) -> T?
): RendererBackendSelection<T> {
    val failures = mutableListOf<Throwable>()
    val backends = listOf(Engine.Backend.OPENGL, Engine.Backend.VULKAN)

    for (backend in backends) {
        try {
            val value = initialize(backend)
            if (value != null) return RendererBackendSelection(backend, value)
            failures += IllegalStateException("Filament ${backend.displayName()} initialization returned no result")
        } catch (failure: Exception) {
            failures += failure
        } catch (failure: LinkageError) {
            failures += failure
        }
    }

    val failure = IllegalStateException("Filament could not initialize OpenGL ES or its optional Vulkan fallback")
    failures.forEach(failure::addSuppressed)
    throw failure
}

internal fun Engine.Backend.displayName(): String = when (this) {
    Engine.Backend.OPENGL -> "OpenGL ES"
    Engine.Backend.VULKAN -> "Vulkan"
    else -> name
}
