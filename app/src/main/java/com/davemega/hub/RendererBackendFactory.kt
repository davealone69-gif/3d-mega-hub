package com.davemega.hub

import com.google.android.filament.Engine

internal data class RendererBackendSelection<T : Any>(
    val backend: Engine.Backend,
    val value: T
)

/**
 * Initializes backends in preference order and returns the first successful result.
 * The callback should create the complete backend-dependent renderer session so
 * GLES is tried when Vulkan setup fails after engine creation as well.
 */
internal fun <T : Any> createWithRendererBackendFallback(
    initialize: (Engine.Backend) -> T?
): RendererBackendSelection<T> {
    val failures = mutableListOf<Throwable>()
    val backends = listOf(Engine.Backend.VULKAN, Engine.Backend.OPENGL)

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

    val failure = IllegalStateException("Filament could not initialize Vulkan or its OpenGL ES fallback")
    failures.forEach(failure::addSuppressed)
    throw failure
}

internal fun Engine.Backend.displayName(): String = when (this) {
    Engine.Backend.VULKAN -> "Vulkan"
    Engine.Backend.OPENGL -> "OpenGL ES"
    else -> name
}
