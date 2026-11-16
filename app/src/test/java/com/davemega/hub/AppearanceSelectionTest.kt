package com.davemega.hub

import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.FaceParam
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
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

    @Test
    fun bodyChoicesBecomeTheSpecValuesTheyName() {
        for (type in BodyType.entries) {
            val spec = AppearanceSelection(bodyType = type).toSpec()
            assertEquals(type.build, spec.build)
            assertEquals(type.muscle, spec.muscle, 0f)
            assertEquals(type.bodyFat, spec.bodyFat, 0f)
        }
        assertEquals(Build.CURVY, AppearanceSelection(bodyType = BodyType.CURVY).toSpec().build)
        // Five sizes each, in rising order, inside the ranges the engine reads.
        assertEquals(5, BustSize.entries.size)
        assertEquals(5, GluteSize.entries.size)
        assertTrue(BustSize.entries.zipWithNext().all { (a, b) -> a.bust < b.bust })
        assertTrue(GluteSize.entries.zipWithNext().all { (a, b) -> a.glute < b.glute })
        assertTrue(BustSize.entries.all { it.bust in 0f..1f })
        assertTrue(GluteSize.entries.all { it.glute in -1f..1f })
        val spec = AppearanceSelection(bust = BustSize.HUGE, glute = GluteSize.SMALL).toSpec()
        assertEquals(BustSize.HUGE.bust, spec.bust, 0f)
        assertEquals(GluteSize.SMALL.glute, spec.glute, 0f)
    }

    @Test
    fun everyAvatarOfTheCreatorIsAnAdult() {
        for (type in BodyType.entries) {
            for (bust in BustSize.entries) {
                val spec = AppearanceSelection(bodyType = type, bust = bust, glute = GluteSize.HUGE).toSpec()
                assertTrue("the creator must build adults only", spec.ageYears >= 18f)
            }
        }
        assertEquals(AppearanceSelection.ADULT_AGE_YEARS, AppearanceSelection().toSpec().ageYears, 0f)
    }

    @Test
    fun aLookSetsSkinHairEyesAndFaceAndNothingElse() {
        val before = AppearanceSelection(sex = Sex.FEMALE, bodyType = BodyType.CURVY, hairStyle = HairStyle.BOB, heightM = 1.62f)
        for (look in Ethnicity.entries) {
            val after = look.applyTo(before)
            assertEquals(look, after.ethnicity)
            assertEquals(look.melanin, after.skinMelanin, 0f)
            assertEquals(look.hairColour, AppearanceCatalogue.hairColours[after.hairColour].label)
            assertEquals(look.eyeColour, AppearanceCatalogue.eyeColours[after.eyeColour].label)
            assertEquals(AppearanceSelection.FACE_SLIDERS, after.face.size)
            assertTrue(after.face.all { it in -1f..1f })
            // What a look does not touch.
            assertEquals(before.bodyType, after.bodyType)
            assertEquals(before.hairStyle, after.hairStyle)
            assertEquals(before.heightM, after.heightM, 0f)
            assertEquals(before.outfit, after.outfit)
            val spec = after.toSpec()
            assertEquals(FaceParam.COUNT, spec.face.size)
            assertArrayEquals(AppearanceCatalogue.eyeColours[after.eyeColour].rgb(), spec.eyeTone, 0f)
            for (param in FaceParam.entries) assertEquals(after.face[param.index], spec.face[param.index], 0f)
            spec.requireGeneratable(spec.prompt)
        }
        assertEquals("Latino", Ethnicity.LATINA.labelFor(Sex.MALE))
        assertEquals("Latina", Ethnicity.LATINA.labelFor(Sex.FEMALE))
        assertEquals(6, Ethnicity.entries.size)
    }

    @Test
    fun aFaceSliderMovesOneValueAndStaysInRange() {
        val moved = AppearanceSelection().withFace(FaceParam.LIP_FULLNESS, 5f)
        assertEquals(1f, moved.face[FaceParam.LIP_FULLNESS.index], 0f)
        assertEquals(0f, moved.face[FaceParam.NOSE_WIDTH.index], 0f)
        assertEquals(1f, moved.toSpec().face[FaceParam.LIP_FULLNESS.index], 0f)
        // The engine's reserved slot is never written.
        assertEquals(0f, moved.toSpec().face[FaceParam.COUNT - 1], 0f)
    }

    @Test
    fun everyHairStyleIsListedOnceInTheCreator() {
        assertEquals(HairStyle.entries.toSet(), CreatorLabels.hairOrder.toSet())
        assertEquals(HairStyle.entries.size, CreatorLabels.hairOrder.size)
        // The eight of the owner's reference lead, in its order.
        assertEquals(
            listOf("Straight", "Bangs", "Braids", "Curly", "Bun", "Ponytail", "Bob", "Bald"),
            CreatorLabels.hairOrder.take(8).map { CreatorLabels.hair(it) }
        )
        assertEquals(listOf("Auto", "Brown", "Black", "Blonde", "Ginger", "Pink"), AppearanceCatalogue.hairColours.take(6).map { it.label })
    }
}
