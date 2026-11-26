package com.davemega.hub

import java.net.URI
import java.util.Locale

/**
 * Validates the base URL used for local-model traffic.
 *
 * Plain HTTP is intentionally limited to loopback so prompts cannot silently
 * leave the device over an unencrypted network. Remote endpoints must use HTTPS.
 */
internal object OllamaEndpoint {
    private val loopbackHosts = setOf("localhost", "127.0.0.1")

    fun normalize(raw: String): String {
        val value = raw.trim()
        require(value.isNotEmpty()) { "Enter an Ollama endpoint URL." }
        require(value.length <= 2_048) { "The Ollama endpoint URL is too long." }

        val uri = try {
            URI(value)
        } catch (_: Exception) {
            throw IllegalArgumentException("The Ollama endpoint URL is invalid.")
        }

        val scheme = uri.scheme?.lowercase(Locale.ROOT)
        require(scheme == "http" || scheme == "https") {
            "Use an http:// loopback URL or an https:// endpoint."
        }
        require(uri.isAbsolute && !uri.isOpaque) { "Enter a complete endpoint URL." }
        require(uri.rawUserInfo == null) { "Credentials must not be embedded in the endpoint URL." }
        require(uri.rawQuery == null && uri.rawFragment == null) {
            "The endpoint URL must not contain a query string or fragment."
        }
        require(uri.rawPath.isNullOrEmpty() || uri.rawPath == "/") {
            "Enter the Ollama server base URL, without an API path."
        }

        val host = uri.host?.lowercase(Locale.ROOT)
            ?: throw IllegalArgumentException("The endpoint URL must contain a valid host.")
        val port = uri.port
        require(port == -1 || port in 1..65_535) { "The endpoint port is invalid." }

        val isLoopback = host in loopbackHosts
        require(scheme == "https" || isLoopback) {
            "Unencrypted HTTP is allowed only for localhost. Use HTTPS for remote servers."
        }

        val authority = buildString {
            append(host)
            if (port != -1) {
                append(':')
                append(port)
            }
        }
        return "$scheme://$authority"
    }

    fun validateModel(raw: String): String {
        val model = raw.trim()
        require(model.isNotEmpty()) { "Enter an Ollama model name." }
        require(model.length <= 128) { "The model name must be 128 characters or fewer." }
        require(model.none { it.isWhitespace() || it.isISOControl() }) {
            "The model name must not contain whitespace or control characters."
        }
        return model
    }
}
