package com.davemega.hub.avatarforge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class AvatarSpecTest {
    @Test
    fun defaultSpecPacksAbiV1FieldCountsAndSeed() {
        val spec = AvatarSpec(seed = 42)
        assertEquals(7, spec.packInts().size)
        assertEquals(33, spec.packFloats().size)
        assertEquals(42, spec.packInts()[6])
        assertEquals(AvatarSpec.DEFAULT_HEIGHT_M, spec.packFloats()[0])
    }

    @Test
    fun unpackRoundTripsEnumsAndMeasurements() {
        val source = AvatarSpec(
            sex = Sex.FEMALE,
            build = Build.ATHLETIC,
            hairStyle = HairStyle.BRAIDS,
            outfit = Outfit.SUIT,
            footwear = Footwear.BOOT,
            seed = 9,
            heightM = 1.68f,
            waistCm = 72f,
            prompt = "armored explorer",
        )
        val decoded = AvatarSpec.unpack(source.packInts(), source.packFloats(), source.prompt)
        assertEquals(source.sex, decoded.sex)
        assertEquals(source.build, decoded.build)
        assertEquals(source.hairStyle, decoded.hairStyle)
        assertEquals(source.outfit, decoded.outfit)
        assertEquals(source.footwear, decoded.footwear)
        assertEquals(source.seed, decoded.seed)
        assertEquals(source.heightM, decoded.heightM, 0.00001f)
        assertEquals(source.waistCm, decoded.waistCm, 0.00001f)
        assertEquals(source.prompt, decoded.prompt)
    }

    @Test
    fun heightCentimetresConvertToBoundedMetres() {
        assertEquals(1.8f, AvatarSpec().withHeightCm(180f).heightM, 0.00001f)
        assertEquals(AvatarSpec.MAX_HEIGHT_M, AvatarSpec().withHeightCm(999f).heightM, 0.00001f)
    }

    @Test
    fun anatomicalOutfitRequiresExplicitAdultConfirmation() {
        assertThrows(IllegalArgumentException::class.java) {
            AvatarSpec(outfit = Outfit.NONE).requireGeneratable()
        }
        assertThrows(IllegalArgumentException::class.java) {
            AvatarSpec(outfit = Outfit.NONE, ageYears = 20f, adultModeConfirmed = true)
                .requireGeneratable("teen character")
        }
        AvatarSpec(outfit = Outfit.NONE, ageYears = 20f, adultModeConfirmed = true)
            .requireGeneratable("adult character")
    }
}
