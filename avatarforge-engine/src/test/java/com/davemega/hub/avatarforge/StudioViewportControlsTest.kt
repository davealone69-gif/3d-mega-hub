package com.davemega.hub.avatarforge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class StudioViewportControlsTest {

    @Test
    fun cameraAndLightingPresetsHaveUniqueNativeIds() {
        val camIds = CameraPreset.entries.map { it.nativeId }.toSet()
        val lightIds = LightingPreset.entries.map { it.nativeId }.toSet()
        assertEquals(CameraPreset.entries.size, camIds.size)
        assertEquals(LightingPreset.entries.size, lightIds.size)
        assertEquals(0, CameraPreset.FULL_BODY.nativeId)
        assertEquals(1, CameraPreset.PORTRAIT.nativeId)
        assertEquals(5, CameraPreset.BUST.nativeId)
        assertEquals(6, CameraPreset.EYES_MACRO.nativeId)
        assertEquals(0, LightingPreset.STUDIO.nativeId)
        assertEquals(4, LightingPreset.REMBRANDT.nativeId)
        assertEquals(5, LightingPreset.NOIR_RIM.nativeId)
    }

    @Test
    fun customizationStateBuildsDeterministicAvatarSpecAndPrompt() {
        val state = AvatarCustomizationState(
            sex = Sex.FEMALE,
            build = Build.ATHLETIC,
            hairStyle = HairStyle.LONG,
            outfit = Outfit.ARMOR_PLATE,
            heightCm = 180,
            skinMelanin = 0.42f,
            seed = 4242,
            customNotes = "silver visor accent",
        )
        val spec = state.toAvatarSpec()
        assertEquals(Sex.FEMALE, spec.sex)
        assertEquals(Build.ATHLETIC, spec.build)
        assertEquals(HairStyle.LONG, spec.hairStyle)
        assertEquals(Outfit.ARMOR_PLATE, spec.outfit)
        assertEquals(Footwear.SABATON, spec.footwear)
        assertEquals(1.80f, spec.heightM, 1e-4f)
        assertEquals(0.42f, spec.skinMelanin, 1e-4f)
        assertEquals(4242, spec.seed)
        assertTrue(spec.prompt.contains("180 cm tall"))
        assertTrue(spec.prompt.contains("long hair"))
        assertTrue(spec.prompt.contains("plate armor"))
        assertTrue(spec.prompt.contains("silver visor accent"))
    }

    @Test
    fun customizationStateClampsExtremeHeightMelaninAndSeed() {
        val state = AvatarCustomizationState(
            heightCm = 999,
            skinMelanin = 5.0f,
            seed = -10,
        )
        val spec = state.toAvatarSpec()
        assertEquals(2.15f, spec.heightM, 1e-4f)
        assertEquals(0.95f, spec.skinMelanin, 1e-4f)
        assertEquals(1, spec.seed)
    }

    @Test
    fun animationClipsAndFacialExpressionsMatchNativeIds() {
        val clipIds = AnimationClipPreset.entries.map { it.nativeId }
        assertEquals((0 until 16).toList(), clipIds)
        assertEquals(0, AnimationClipPreset.REST.nativeId)
        assertEquals(3, AnimationClipPreset.WALK.nativeId)
        assertEquals(14, AnimationClipPreset.TURN_360.nativeId)
        assertEquals(15, AnimationClipPreset.PUSHUP.nativeId)

        val exprIds = FacialExpressionPreset.entries.map { it.nativeId }
        assertEquals((0 until FacialExpressionPreset.entries.size).toList(), exprIds)
        assertEquals(0, FacialExpressionPreset.NEUTRAL.nativeId)
        assertEquals(1, FacialExpressionPreset.SMILE.nativeId)
        assertEquals(7, FacialExpressionPreset.GAZE_RIGHT.nativeId)
    }

    @Test
    fun avatarRegionNamesCoverAllSixteenNativeRegions() {
        val names = (0 until 16).map { AvatarRegionNames.displayName(it) }
        assertEquals(16, names.toSet().size)
        assertEquals("Skin", AvatarRegionNames.displayName(0))
        assertEquals("Left Cornea", AvatarRegionNames.displayName(13))
        assertEquals("Right Cornea", AvatarRegionNames.displayName(14))
        assertEquals("Hair Cards", AvatarRegionNames.displayName(15))
        assertEquals("Imported Mesh", AvatarRegionNames.displayName(-1))
    }
}
