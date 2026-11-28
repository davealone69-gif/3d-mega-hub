package com.davemega.hub.avatarforge

/**
 * Parses and validates the native renderer descriptor returned by `af::Renderer::info()`.
 *
 * Native descriptors use pipe-delimited tokens:
 * - Vulkan: `vulkan|<deviceName>|api <major>.<minor>`
 * - OpenGL ES 3.1/3.2: `gles31|<glVersion>|<glRenderer>`
 */
object RendererBackendPolicy {
    enum class Backend(val id: String, val displayName: String) {
        VULKAN("vulkan", "Vulkan"),
        GLES31("gles31", "OpenGL ES 3.1"),
    }

    data class RendererDescriptor(
        val backend: Backend,
        val apiVersion: String,
        val deviceSummary: String,
        val rawInfo: String,
    )

    fun parse(rendererInfo: String): RendererDescriptor {
        val parts = rendererInfo.split('|')
        val prefix = parts.firstOrNull()?.trim().orEmpty()
        val backend = when (prefix) {
            Backend.VULKAN.id -> Backend.VULKAN
            Backend.GLES31.id -> Backend.GLES31
            else -> throw IllegalStateException(
                "Unexpected AvatarForge backend information: $rendererInfo"
            )
        }
        val second = parts.getOrNull(1)?.trim().orEmpty()
        val third = parts.getOrNull(2)?.trim().orEmpty()
        return when (backend) {
            Backend.VULKAN -> RendererDescriptor(
                backend = backend,
                apiVersion = third.ifBlank { "Vulkan" },
                deviceSummary = second.ifBlank { "Vulkan GPU" },
                rawInfo = rendererInfo,
            )
            Backend.GLES31 -> RendererDescriptor(
                backend = backend,
                apiVersion = second.ifBlank { "OpenGL ES 3.1" },
                deviceSummary = third.ifBlank { second.ifBlank { "OpenGL ES GPU" } },
                rawInfo = rendererInfo,
            )
        }
    }

    fun isVulkan(rendererInfo: String): Boolean = rendererInfo.startsWith("${Backend.VULKAN.id}|")
}
