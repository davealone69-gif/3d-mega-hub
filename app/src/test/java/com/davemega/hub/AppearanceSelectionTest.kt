package com.davemega.hub

import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AppearanceSelectionTest {
    @Test
    fun everyOfferedPresetBecomesTheSameExplicitSpecValue() {
        for (hair in HairStyle.entries) {
            for (outfit in AppearanceCatalogue.outfits) {
                val spec = AppearanceSelection(hairStyle = hair, outfit = outfit, footwear = Footwear.BOOT).toSpec()
                assertEquals(hair, spec.hairStyle)
                assertEquals(outfit, spec.outfit)
                assertEquals(Footwear.BOOT, spec.footwear)
                assertTrue("the outfit must be marked explicit so the prompt cannot replace it", spec.outfitExplicit)
                // Throws when the spec could not be generated (for example an unclothed base).
                spec.requireGeneratable(spec.prompt)
            }
        }
    }

    @Test
    fun theUnclothedBaseIsNotOffered() {
        assertFalse(AppearanceCatalogue.outfits.contains(Outfit.NONE))
        assertEquals(Outfit.entries.size - 1, AppearanceCatalogue.outfits.size)
    }

    @Test
    fun thePresetPromptNamesNoPreset() {
        // Words the engine's prompt parser maps to a hair style, an outfit, a build or footwear.
        val parserWords = setOf(
            "bald", "shaved", "buzz", "buzzcut", "short", "medium", "long", "ponytail", "bun", "braid", "braids",
            "afro", "mohawk", "tshirt", "t-shirt", "tee", "shirt", "dress", "gown", "suit", "jeans", "denim",
            "jacket", "armour", "armor", "plate", "leather", "spacesuit", "space", "athletic", "coat", "boots",
            "sneakers", "heels", "man", "woman", "male", "female", "slim", "muscular", "heavy", "average"
        )
        val words = Regex("[A-Za-z-]+").findAll(AppearanceCatalogue.PRESET_PROMPT).map { it.value.lowercase() }.toList()
        assertTrue(words.isNotEmpty())
        assertTrue(words.none { it in parserWords })
        assertTrue(AppearanceCatalogue.PRESET_PROMPT.length <= 500)
    }

    @Test
    fun coloursAndRangesAreWhatTheSpecAccepts() {
        val spec = AppearanceSelection(hairColour = 4, clothColour = 3, heightM = 9f, skinMelanin = -2f).toSpec()
        assertArrayEquals(AppearanceCatalogue.hairColours[4].rgb(), spec.hairTone, 0f)
        assertArrayEquals(AppearanceCatalogue.clothColours[3].rgb(), spec.clothTone, 0f)
        assertEquals(AppearanceCatalogue.MAX_HEIGHT_M, spec.heightM, 0f)
        assertEquals(0f, spec.skinMelanin, 0f)
        // Index 0 is "engine default": all zero, which the engine reads as "not set".
        assertArrayEquals(floatArrayOf(0f, 0f, 0f), AppearanceSelection().toSpec().hairTone, 0f)
        // An out-of-range index must not crash the screen.
        AppearanceSelection(hairColour = 99, clothColour = -5).toSpec()
    }
}
