package com.davemega.hub.avatarforge

import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets
import org.junit.Assert.assertTrue
import org.junit.Assert.assertThrows
import org.junit.Test

class GlbValidatorMorphCompatibilityTest {
    @Test
    fun rejectsDifferentMorphTargetCountsWithinOneMesh() {
        val json = gltf(
            accessors = listOf(
                accessor(3, "VEC3"),
                accessor(3, "SCALAR", 5123),
                accessor(3, "VEC3"),
                accessor(3, "VEC3"),
                accessor(3, "SCALAR", 5123)
            ),
            primitives = listOf(
                primitive(position = 0, indices = 1, morphPositionAccessors = listOf(2)),
                primitive(position = 3, indices = 4)
            )
        )

        val failure = assertThrows(IllegalArgumentException::class.java) {
            validateGlb(json)
        }

        assertTrue(failure.message.orEmpty().contains("morph target count"))
    }

    @Test
    fun rejectsPrimitiveWithNoIndices() {
        val json = gltf(
            accessors = listOf(accessor(3, "VEC3"), accessor(0, "SCALAR", 5123)),
            primitives = listOf(primitive(position = 0, indices = 1))
        )

        val failure = assertThrows(IllegalArgumentException::class.java) {
            validateGlb(json)
        }

        assertTrue(failure.message.orEmpty().contains("zero indices"))
    }

    @Test
    fun acceptsSisterPrimitivesWithMatchingMorphTargets() {
        val json = gltf(
            accessors = listOf(
                accessor(3, "VEC3"),
                accessor(3, "SCALAR", 5123),
                accessor(3, "VEC3"),
                accessor(3, "VEC3"),
                accessor(3, "SCALAR", 5123),
                accessor(3, "VEC3")
            ),
            primitives = listOf(
                primitive(position = 0, indices = 1, morphPositionAccessors = listOf(2)),
                primitive(position = 3, indices = 4, morphPositionAccessors = listOf(5))
            )
        )

        validateGlb(json)
    }

    private fun validateGlb(json: String) {
        val file = File.createTempFile("glb-morph-compatibility-", ".glb")
        try {
            file.writeBytes(encodeGlb(json))
            GlbValidator.validate(file)
        } finally {
            file.delete()
        }
    }

    private fun encodeGlb(json: String): ByteArray {
        val jsonBytes = json.toByteArray(StandardCharsets.UTF_8)
        val paddedJsonLength = (jsonBytes.size + 3) and 3.inv()
        val totalLength = 12 + 8 + paddedJsonLength
        return ByteBuffer.allocate(totalLength).order(ByteOrder.LITTLE_ENDIAN).apply {
            putInt(0x46546C67)
            putInt(2)
            putInt(totalLength)
            putInt(paddedJsonLength)
            putInt(0x4E4F534A)
            put(jsonBytes)
            repeat(paddedJsonLength - jsonBytes.size) { put(0x20) }
        }.array()
    }

    private fun gltf(accessors: List<String>, primitives: List<String>): String =
        """{"asset":{"version":"2.0"},"accessors":[${accessors.joinToString(",")}],"meshes":[{"primitives":[${primitives.joinToString(",")}]}]}"""

    private fun accessor(count: Int, type: String, componentType: Int = 5126): String =
        """{"componentType":$componentType,"count":$count,"type":"$type"}"""

    private fun primitive(
        position: Int,
        indices: Int,
        morphPositionAccessors: List<Int> = emptyList()
    ): String {
        val targets = if (morphPositionAccessors.isEmpty()) "" else
            ""","targets":[${morphPositionAccessors.joinToString(",") { """{"POSITION":$it}""" }}]"""
        return """{"attributes":{"POSITION":$position},"indices":$indices$targets}"""
    }
}
