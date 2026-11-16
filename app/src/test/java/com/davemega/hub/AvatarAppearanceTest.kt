package com.davemega.hub

import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class AvatarAppearanceTest {
    @Test
    fun mapsEveryControlOntoTheEngineSpec() {
        val appearance = AvatarAppearance(
            sex = Sex.FEMALE,
            build = Build.ATHLETIC,
            hairStyle = HairStyle.BRAIDS,
            outfit = Outfit.DRESS,
            hairColour = HairColour.AUBURN,
            clothColour = ClothColour.CRIMSON,
            skinMelanin = 0.72f,
            heightCm = 168f,
            ageYears = 41f
        )

        val spec = appearance.toSpec(prompt = "adult woman", seed = 7)

        assertEquals(Sex.FEMALE, spec.sex)
        assertEquals(Build.ATHLETIC, spec.build)
        assertEquals(HairStyle.BRAIDS, spec.hairStyle)
        assertEquals(Outfit.DRESS, spec.outfit)
        assertEquals(0.72f, spec.skinMelanin, 1e-6f)
        assertEquals(1.68f, spec.heightM, 1e-6f)
        assertEquals(41f, spec.ageYears, 1e-6f)
        assertEquals(7, spec.seed)
        assertEquals("adult woman", spec.prompt)
        assertArrayEquals(HairColour.AUBURN.toLinearRgb(), spec.hairTone, 1e-6f)
        assertArrayEquals(ClothColour.CRIMSON.toLinearRgb(), spec.clothTone, 1e-6f)
        assertTrue(spec.outfitExplicit)
    }

    @Test
    fun naturalHairAndNeutralClothStayZeroSoTheEngineDerivesThem() {
        val spec = AvatarAppearance().toSpec()

        assertArrayEquals(floatArrayOf(0f, 0f, 0f), spec.hairTone, 0f)
        assertArrayEquals(floatArrayOf(0f, 0f, 0f), spec.clothTone, 0f)
    }

    @Test
    fun everyColourPresetKeepsThreeLinearChannels() {
        for (colour in HairColour.entries) {
            val rgb = colour.toLinearRgb()
            assertEquals("hair ${colour.name}", 3, rgb.size)
            assertTrue("hair ${colour.name} stays in 0..1", rgb.all { it >= 0f && it <= 1f })
        }
        for (colour in ClothColour.entries) {
            val rgb = colour.toLinearRgb()
            assertEquals("cloth ${colour.name}", 3, rgb.size)
            assertTrue("cloth ${colour.name} stays in 0..1", rgb.all { it >= 0f && it <= 1f })
        }
    }

    @Test
    fun outOfRangeAndNonFiniteValuesAreClamped() {
        val clamped = AvatarAppearance(
            skinMelanin = 4.5f,
            heightCm = 900f,
            ageYears = 3f
        ).normalized()

        assertEquals(1f, clamped.skinMelanin, 0f)
        assertEquals(220f, clamped.heightCm, 1e-4f)
        assertEquals(18f, clamped.ageYears, 0f)

        val repaired = AvatarAppearance(
            skinMelanin = Float.NaN,
            heightCm = Float.NEGATIVE_INFINITY,
            ageYears = Float.NaN
        ).normalized()

        assertEquals(0.35f, repaired.skinMelanin, 1e-6f)
        assertEquals(175f, repaired.heightCm, 1e-6f)
        assertEquals(30f, repaired.ageYears, 1e-6f)
    }

    @Test
    fun unclothedOutfitStillRequiresTheAdultGate() {
        val nude = AvatarAppearance(outfit = Outfit.NONE)

        assertThrows(IllegalArgumentException::class.java) { nude.toSpec() }
        assertFalse(nude.outfit in AvatarAppearance.selectableOutfits)
    }

    @Test
    fun selectableOutfitsExcludeTheUnclothedSurface() {
        assertEquals(Outfit.entries.size - 1, AvatarAppearance.selectableOutfits.size)
        assertFalse(Outfit.NONE in AvatarAppearance.selectableOutfits)
    }

    @Test
    fun describeReportsWhatWillBeGenerated() {
        val text = AvatarAppearance(
            sex = Sex.FEMALE,
            hairStyle = HairStyle.LONG,
            hairColour = HairColour.BLONDE,
            outfit = Outfit.COAT,
            skinMelanin = 0.2f,
            heightCm = 172f
        ).describe()

        assertTrue(text, text.contains("female"))
        assertTrue(text, text.contains("long hair"))
        assertTrue(text, text.contains("blonde"))
        assertTrue(text, text.contains("coat"))
        assertTrue(text, text.contains("172 cm"))
        assertTrue(text, text.contains("0.20"))
    }

    @Test
    fun appearanceSurvivesPersistenceRoundTrip() {
        val original = AvatarAppearance(
            sex = Sex.MALE,
            build = Build.MUSCULAR,
            hairStyle = HairStyle.AFRO,
            outfit = Outfit.SUIT,
            hairColour = HairColour.BLACK,
            clothColour = ClothColour.INK,
            skinMelanin = 0.61f,
            heightCm = 188f,
            ageYears = 55f
        )

        val restored = AppearanceStore.decode(AppearanceStore.encode(original).let { stored ->
            { key: String -> stored[key] }
        })

        assertEquals(original.normalized(), restored)
    }

    @Test
    fun unreadableStoredValuesFallBackToDefaults() {
        val stored = mapOf(
            "appearance_sex" to "NOT_A_SEX",
            "appearance_hair_style" to "",
            "appearance_melanin" to "not-a-number",
            "appearance_height_cm" to "NaN"
        )

        val restored = AppearanceStore.decode { key -> stored[key] }

        assertEquals(AvatarAppearance(), restored)
    }
}
