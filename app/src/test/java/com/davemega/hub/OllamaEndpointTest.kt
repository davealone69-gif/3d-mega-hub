package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class OllamaEndpointTest {
    @Test
    fun loopbackHttpIsAllowedAndNormalized() {
        assertEquals("http://localhost:11434", OllamaEndpoint.normalize(" http://localhost:11434/ "))
        assertEquals("http://127.0.0.1:11434", OllamaEndpoint.normalize("http://127.0.0.1:11434"))
    }

    @Test
    fun remoteHttpsIsAllowed() {
        assertEquals("https://ollama.example.com:443", OllamaEndpoint.normalize("HTTPS://Ollama.Example.com:443/"))
    }

    @Test
    fun remoteHttpIsRejected() {
        assertThrows(IllegalArgumentException::class.java) {
            OllamaEndpoint.normalize("http://192.168.1.20:11434")
        }
    }

    @Test
    fun credentialsAndUrlDecorationsAreRejected() {
        listOf(
            "http://user:password@localhost:11434",
            "http://localhost:11434/api",
            "http://localhost:11434/?token=secret",
            "http://localhost:11434/#fragment"
        ).forEach { value ->
            assertThrows(IllegalArgumentException::class.java) { OllamaEndpoint.normalize(value) }
        }
    }

    @Test
    fun unsupportedSchemesAndInvalidPortsAreRejected() {
        listOf("ftp://localhost:21", "file:///tmp/ollama", "http://localhost:70000", "localhost:11434")
            .forEach { value ->
                assertThrows(IllegalArgumentException::class.java) { OllamaEndpoint.normalize(value) }
            }
    }

    @Test
    fun modelNamesAreValidated() {
        assertEquals("llama3.2:1b", OllamaEndpoint.validateModel(" llama3.2:1b "))
        listOf("", " ", "model name", "model\nname", "x".repeat(129)).forEach { value ->
            assertThrows(IllegalArgumentException::class.java) { OllamaEndpoint.validateModel(value) }
        }
    }
}
