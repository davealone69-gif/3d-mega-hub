package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Pins the correspondence between the animation core's clip list and a renderer's animation list.
 *
 * This is the one part of the Filament integration whose correctness can be settled on a JVM: the two
 * parsers are only compared through names and counts, so no GPU, no glTF file and no device is needed.
 * Every expectation below was computed from the shipped algorithm rather than guessed, because a mapping
 * test that asserts what the author hoped for instead of what the code does is worse than no test.
 */
class ClipMappingTest {

    @Test
    fun identicalNamesMatchByNameAndNeedNoFallback() {
        val mapping = mapClipIndices(
            listOf("Survey", "Walk", "Run"),
            listOf("Survey", "Walk", "Run"),
        )
        assertTrue(mapping.complete)
        assertEquals(listOf(0, 1, 2), mapping.nativeToRenderer.toList())
        assertEquals(
            listOf(ClipMatchMethod.NAME, ClipMatchMethod.NAME, ClipMatchMethod.NAME),
            mapping.methods,
        )
        assertTrue("a clean name match produces no explanatory notes", mapping.notes.isEmpty())
    }

    /**
     * The case positional matching exists for: Fox is named, but CesiumMan's single animation is unnamed
     * in the document, so the core labels it `<unnamed animation 0>` while gltfio reports its own label.
     * Neither name identifies the clip, but both parsers read one animation, so document order settles it.
     */
    @Test
    fun unnamedClipsFallBackToPositionWhenCountsAgree() {
        for (rendererLabel in listOf("", "animation_0", "anim-1")) {
            val mapping = mapClipIndices(listOf("<unnamed animation 0>"), listOf(rendererLabel))
            assertEquals(
                "renderer label '$rendererLabel'",
                listOf(0),
                mapping.nativeToRenderer.toList(),
            )
            assertEquals(listOf(ClipMatchMethod.POSITION), mapping.methods)
            assertTrue(
                "a positional match must explain itself",
                mapping.notes.any { it.contains("document position") },
            )
        }
    }

    /**
     * Name matching must survive the two parsers disagreeing about order. If this ever regresses to
     * positional matching, a reordered asset plays the wrong animation with no error anywhere.
     */
    @Test
    fun namesMatchAcrossADifferentDocumentOrder() {
        val mapping = mapClipIndices(listOf("Walk", "Run"), listOf("Run", "Walk"))
        assertEquals(listOf(1, 0), mapping.nativeToRenderer.toList())
        assertEquals(listOf(ClipMatchMethod.NAME, ClipMatchMethod.NAME), mapping.methods)
    }

    /**
     * The dangerous case, and the reason the count guard exists. If the core refuses an unreadable
     * channel it ends up with fewer clips than the renderer, and position N in one list no longer means
     * position N in the other. Names must then carry the whole match, and a nameless clip must be left
     * UNMATCHED rather than silently bound to the wrong animation.
     */
    @Test
    fun differingCountsDisablePositionalMatching() {
        val mapping = mapClipIndices(
            listOf("Walk", "Run", "<unnamed animation 2>"),
            listOf("Walk", "Run"),
        )
        assertEquals(listOf(0, 1, -1), mapping.nativeToRenderer.toList())
        assertEquals(
            listOf(ClipMatchMethod.NAME, ClipMatchMethod.NAME, ClipMatchMethod.UNMATCHED),
            mapping.methods,
        )
        assertFalse("an unmatched clip must not be reported as a complete mapping", mapping.complete)
        assertEquals(2, mapping.matchedCount)
        assertTrue(
            mapping.notes.any { it.contains("disagree on the clip count") },
        )
    }

    /** The mirror image: the CORE has fewer clips than the renderer, and names still route correctly. */
    @Test
    fun coreCanSkipARendererClipWithoutShiftingTheOthers() {
        val mapping = mapClipIndices(listOf("Walk", "Jump"), listOf("Walk", "Run", "Jump"))
        assertEquals(listOf(0, 2), mapping.nativeToRenderer.toList())
        assertEquals(listOf(ClipMatchMethod.NAME, ClipMatchMethod.NAME), mapping.methods)
        assertTrue(
            "the renderer's unclaimed animation is reported, not silently ignored",
            mapping.notes.any { it.contains("'Run'") },
        )
    }

    /** Two identically named renderer animations are ambiguous; the first wins and the ambiguity is said. */
    @Test
    fun duplicateRendererNamesMatchTheFirstOccurrenceAndAreReported() {
        val mapping = mapClipIndices(listOf("Walk", "Run"), listOf("Walk", "Walk", "Run"))
        assertEquals(listOf(0, 2), mapping.nativeToRenderer.toList())
        assertTrue(
            mapping.notes.any { it.contains("more than one animation named") },
        )
    }

    @Test
    fun aRendererAnimationTheCoreDidNotLoadIsReported() {
        val mapping = mapClipIndices(listOf("Walk"), listOf("Walk", "Run"))
        assertEquals(listOf(0), mapping.nativeToRenderer.toList())
        assertTrue(mapping.notes.any { it.contains("the core did not load") })
    }

    @Test
    fun emptySidesProduceAnEmptyMappingWithAReason() {
        val noCoreClips = mapClipIndices(emptyList(), listOf("Walk"))
        assertEquals(0, noCoreClips.nativeToRenderer.size)
        assertFalse(noCoreClips.complete)
        assertTrue(noCoreClips.notes.any { it.contains("loaded no clips") })

        val noRendererAnimations = mapClipIndices(listOf("Walk"), emptyList())
        assertEquals(listOf(-1), noRendererAnimations.nativeToRenderer.toList())
        assertEquals(listOf(ClipMatchMethod.UNMATCHED), noRendererAnimations.methods)
        assertTrue(noRendererAnimations.notes.any { it.contains("reported no animations") })
    }

    /**
     * A clip the core could not name still matches positionally when the counts agree, even though its
     * renderer counterpart DOES have a name: document order is the ground truth and names are only
     * labels. This is what lets an unnamed animation drive a renderer that named it.
     */
    @Test
    fun anUnnamedCoreClipTakesTheFreeSlotWhenCountsAgree() {
        val mapping = mapClipIndices(
            listOf("Walk", "<unnamed animation 1>"),
            listOf("Walk", "Idle"),
        )
        assertEquals(listOf(0, 1), mapping.nativeToRenderer.toList())
        assertEquals(listOf(ClipMatchMethod.NAME, ClipMatchMethod.POSITION), mapping.methods)
    }

    @Test
    fun generatedNameHeuristicSeparatesParserNumberingFromAuthoredNames() {
        for (generated in listOf("", "   ", "<unnamed animation 0>", "animation_0", "anim-1", "animation 2")) {
            assertFalse("'$generated' is a parser's own numbering", isAuthoritativeName(generated))
        }
        assertFalse(isAuthoritativeName(null))
        for (authored in listOf("Walk", "animation_walk", "Idle_Breathing", "anim_run")) {
            assertTrue("'$authored' is a name the asset author chose", isAuthoritativeName(authored))
        }
    }

    /**
     * When the core reports a name twice and the counts agree, the second clip may not reuse the
     * renderer slot the first one claimed. It falls back to its own document position, which is correct:
     * the two parsers read the same animations array, so position is the ground truth and the duplicated
     * name is the thing that is wrong.
     */
    @Test
    fun aDuplicateCoreNameFallsBackToItsOwnDocumentSlot() {
        val mapping = mapClipIndices(listOf("Walk", "Walk"), listOf("Walk", "Run"))
        assertEquals(listOf(0, 1), mapping.nativeToRenderer.toList())
        assertEquals(listOf(ClipMatchMethod.NAME, ClipMatchMethod.POSITION), mapping.methods)
        assertTrue(
            mapping.notes.any { it.contains("document position 1") },
        )
    }

    /**
     * The other duplicate shape: the name match points at a slot another clip already took, and this
     * clip's own position is taken too. Nothing defensible is left, so it stays UNMATCHED and says why.
     * Binding it somewhere anyway would play an animation the user did not ask for.
     */
    @Test
    fun aClaimedRendererAnimationIsNeverReused() {
        val mapping = mapClipIndices(listOf("Walk", "Walk"), listOf("Run", "Walk"))
        assertEquals(listOf(1, -1), mapping.nativeToRenderer.toList())
        assertEquals(listOf(ClipMatchMethod.NAME, ClipMatchMethod.UNMATCHED), mapping.methods)
        assertFalse(mapping.complete)
        assertEquals(1, mapping.matchedCount)
        assertTrue(
            mapping.notes.any { it.contains("already claimed by another clip") },
        )
    }

    /* ------------------------------------------------------------ morph target order --------- */

    @Test
    fun identicalMorphTargetOrderIsAccepted() {
        val names = listOf("jawOpen", "eyeBlinkLeft", "mouthSmile")
        assertNull(verifyMorphTargetOrder(names, names))
    }

    @Test
    fun caseAndPaddingDifferencesAreStillTheSameOrder() {
        assertNull(
            verifyMorphTargetOrder(listOf("jawOpen", "eyeBlinkLeft"), listOf(" jawopen ", "EYEBLINKLEFT")),
        )
    }

    /**
     * Morph weights cross the boundary positionally with no names on the wire, so a reordered target
     * list would apply every expression to the wrong target -- a face that smiles with its eyelids.
     * That must be refused, with the offending index named.
     */
    @Test
    fun reorderedMorphTargetsAreRefused() {
        val reason = verifyMorphTargetOrder(
            listOf("jawOpen", "eyeBlinkLeft"),
            listOf("eyeBlinkLeft", "jawOpen"),
        )
        assertTrue("a reorder must be refused", reason != null)
        assertTrue(reason!!.contains("morph target 0"))
    }

    @Test
    fun differingMorphTargetCountsAreRefused() {
        val reason = verifyMorphTargetOrder(listOf("jawOpen"), listOf("jawOpen", "eyeBlinkLeft"))
        assertTrue(reason != null)
        assertTrue(reason!!.contains("counts differ"))
    }

    /** An asset with no named targets has no ordering to get wrong, and no weights to push either way. */
    @Test
    fun emptyMorphTargetListsAreNotAnOrderingConflict() {
        assertNull(verifyMorphTargetOrder(emptyList(), listOf("jawOpen")))
        assertNull(verifyMorphTargetOrder(listOf("jawOpen"), emptyList()))
        assertNull(verifyMorphTargetOrder(emptyList(), emptyList()))
    }

    /** A blank name on one side carries no identity, so it cannot contradict the other side. */
    @Test
    fun unnamedMorphTargetsDoNotContradictNamedOnes() {
        assertNull(verifyMorphTargetOrder(listOf("", "eyeBlinkLeft"), listOf("jawOpen", "eyeBlinkLeft")))
    }
}
