package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GlbClipNamesTest {
    @Test
    fun namesWrittenInTheFileAreKeptInOrder() {
        assertEquals(
            listOf("Jumping jacks", "Hands to hair", "T-pose"),
            GlbClipNames.displayNames(listOf("Jumping jacks", "Hands to hair", "T-pose"))
        )
    }

    @Test
    fun aClipWithoutANameIsCalledByItsPlaceInTheFile() {
        assertEquals(
            listOf("Walk", "Clip 2", "Clip 3", "Clip 4"),
            GlbClipNames.displayNames(listOf("Walk", "", null, "   "))
        )
    }

    @Test
    fun spacesAtTheEndsAndRunsOfWhiteSpaceAreRemoved() {
        assertEquals(
            listOf("Dance 2 arm raised", "Idle"),
            GlbClipNames.displayNames(listOf("  Dance  2\tarm\n raised ", "\u0000Idle\u0007"))
        )
    }

    @Test
    fun repeatedNamesAreNumberedSoThatEveryLabelIsDifferent() {
        assertEquals(
            listOf("Animation", "Animation (2)", "Animation (3)", "Walk"),
            GlbClipNames.displayNames(listOf("Animation", "Animation", " Animation ", "Walk"))
        )
    }

    @Test
    fun aGeneratedLabelDoesNotCollideWithANameFromTheFile() {
        assertEquals(
            listOf("Clip 2", "Clip 2 (2)", "Clip 2 (2) (2)"),
            GlbClipNames.displayNames(listOf("Clip 2", "", "Clip 2 (2)"))
        )
    }

    @Test
    fun aVeryLongNameIsCutAndEndsInAnEllipsis() {
        val long = "Armature|mixamo.com|Layer0 retargeted from the original capture session 14"
        val label = GlbClipNames.displayNames(listOf(long)).single()
        assertEquals(GlbClipNames.MAX_LENGTH, label.length)
        assertTrue(label.endsWith("\u2026"))
        assertTrue(long.startsWith(label.dropLast(1)))
    }

    @Test
    fun twoLongNamesWithTheSameBeginningStayDifferent() {
        val first = "A".repeat(60) + " one"
        val second = "A".repeat(60) + " two"
        val labels = GlbClipNames.displayNames(listOf(first, second))
        assertEquals(2, labels.toSet().size)
        assertEquals(labels[0] + " (2)", labels[1])
    }

    @Test
    fun theListKeepsItsLengthSoALabelIndexIsAClipIndex() {
        assertEquals(emptyList<String>(), GlbClipNames.displayNames(emptyList()))
        assertEquals(15, GlbClipNames.displayNames(List(15) { null }).size)
        assertEquals("Clip 15", GlbClipNames.displayNames(List(15) { null }).last())
    }
}
