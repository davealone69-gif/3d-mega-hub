package com.davemega.hub

import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CharacterPresetsTest {
    @Test
    fun presetsHaveStableUniqueIdsAndSafePrompts() {
        val presets = CharacterPresets.all
        assertEquals(listOf("scout", "sentinel"), presets.map { it.id })
        assertEquals(presets.size, presets.map { it.id }.distinct().size)
        presets.forEach { preset ->
            assertEquals(preset, CharacterPresets.find(preset.id))
            assertTrue(preset.prompt.isNotBlank())
            assertTrue(preset.prompt.length <= 500)
            assertEquals(preset.prompt, preset.createSpec().prompt)
            preset.createSpec().requireGeneratable()
        }
        assertNull(CharacterPresets.find(null))
        assertNull(CharacterPresets.find("unknown"))
    }

    @Test
    fun scoutAndSentinelUseTheSupportedGeneratorFeatures() {
        val scout = CharacterPresets.SCOUT.createSpec()
        assertEquals(Sex.FEMALE, scout.sex)
        assertEquals(Build.ATHLETIC, scout.build)
        assertEquals(HairStyle.PONYTAIL, scout.hairStyle)
        assertEquals(Outfit.ARMOR_LEATHER, scout.outfit)
        assertEquals(Footwear.BOOT, scout.footwear)
        assertEquals(1.75f, scout.heightM, 0.001f)
        assertEquals(4101, scout.seed)

        val sentinel = CharacterPresets.SENTINEL.createSpec()
        assertEquals(Sex.MALE, sentinel.sex)
        assertEquals(Build.ATHLETIC, sentinel.build)
        assertEquals(HairStyle.SHORT, sentinel.hairStyle)
        assertEquals(Outfit.ARMOR_PLATE, sentinel.outfit)
        assertEquals(Footwear.BOOT, sentinel.footwear)
        assertEquals(1.82f, sentinel.heightM, 0.001f)
        assertEquals(4102, sentinel.seed)

        // These exact prompts are also exercised against the native parser in
        // character_preset_prompt_test.cpp. Update both tests when changing a preset.
        assertEquals("Adult woman, athletic build, 175 cm tall, ponytail, leather armor and boots", CharacterPresets.SCOUT.prompt)
        assertEquals("Adult man, athletic build, 182 cm tall, short hair, plate armor and boots", CharacterPresets.SENTINEL.prompt)
    }

    @Test
    fun editedOrStaleDraftDoesNotInheritHiddenPresetParameters() {
        val scout = CharacterPresets.SCOUT
        val draft = scout.initialDraft()
        val selected = CharacterPresets.specForDraft(scout.id, " ${scout.prompt} ", draft)
        assertEquals(scout.createSpec().seed, selected.seed)
        assertEquals(Outfit.ARMOR_LEATHER, selected.outfit)

        val edited = CharacterPresets.specForDraft(scout.id, "Adult man in a suit", draft)
        assertEquals("Adult man in a suit", edited.prompt)
        assertEquals(Sex.UNSPECIFIED, edited.sex)
        assertEquals(Outfit.TSHIRT, edited.outfit)
        assertFalse(edited.outfitExplicit)
        val noSelection = CharacterPresets.specForDraft(null, scout.prompt, draft)
        assertEquals(Outfit.TSHIRT, noSelection.outfit)
        val unknownSelection = CharacterPresets.specForDraft("missing", scout.prompt, draft)
        assertEquals(Outfit.TSHIRT, unknownSelection.outfit)
        val mismatchedDraft = CharacterPresets.specForDraft(scout.id, scout.prompt, CharacterPresets.SENTINEL.initialDraft())
        assertEquals(Outfit.TSHIRT, mismatchedDraft.outfit)
    }

    @Test
    fun eachGenerationGetsIndependentDeterministicColourArrays() {
        val first = CharacterPresets.SCOUT.createSpec()
        val second = CharacterPresets.SCOUT.createSpec()
        assertNotSame(first.hairTone, second.hairTone)
        assertNotSame(first.gearTone, second.gearTone)
        assertArrayEquals(first.packInts(), second.packInts())
        assertArrayEquals(first.packFloats(), second.packFloats(), 0f)
        first.hairTone[0] = 0.99f
        assertEquals(0.045f, second.hairTone[0], 0.0001f)
    }

    @Test
    fun editorChangesFlowIntoTheSpecAndItsConsistentPrompt() {
        val preset = CharacterPresets.SCOUT
        val initial = preset.initialDraft()
        assertEquals(preset.prompt, initial.prompt())
        assertEquals(CharacterPresets.SENTINEL.prompt, CharacterPresets.SENTINEL.initialDraft().prompt())
        val draft = initial.copy(
            sex = Sex.MALE,
            build = Build.SLIM,
            hair = HairStyle.BRAIDS,
            outfit = Outfit.JEANS_JACKET,
            footwear = Footwear.SNEAKER,
            heightCm = 168,
            skin = SkinChoice.DEEP,
            hairColor = HairColorChoice.SILVER,
        )
        val spec = CharacterPresets.specForDraft(preset.id, draft.prompt(), draft)
        assertEquals(Sex.MALE, spec.sex)
        assertEquals(Build.SLIM, spec.build)
        assertEquals(HairStyle.BRAIDS, spec.hairStyle)
        assertEquals(Outfit.JEANS_JACKET, spec.outfit)
        assertEquals(Footwear.SNEAKER, spec.footwear)
        assertEquals(1.68f, spec.heightM, 0.001f)
        assertEquals(SkinChoice.DEEP.melanin, spec.skinMelanin, 0.001f)
        assertEquals(HairColorChoice.SILVER.red, spec.hairTone[0], 0.001f)
        assertEquals("Adult man, slim build, 168 cm tall, braids, jacket and jeans and sneakers", draft.prompt())
        spec.requireGeneratable()
    }
}
