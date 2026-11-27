package com.davemega.hub.avatarforge

import kotlin.math.roundToInt

/**
 * Curated camera framing presets matching `nativeSetCameraPreset` in `avatarforge_renderer_jni.cpp`.
 */
enum class CameraPreset(val nativeId: Int, val label: String) {
    FULL_BODY(0, "Full"),
    PORTRAIT(1, "Portrait"),
    THREE_QUARTER(2, "3/4"),
    PROFILE(3, "Side"),
    BACK(4, "Back"),
}

/**
 * Curated HDR studio lighting presets matching `nativeSetLightingPreset` in `avatarforge_renderer_jni.cpp`.
 */
enum class LightingPreset(val nativeId: Int, val label: String) {
    STUDIO(0, "Studio"),
    GOLDEN_HOUR(1, "Golden Hour"),
    DRAMATIC_RIM(2, "Dramatic Rim"),
    OVERCAST(3, "Overcast"),
}

/**
 * Built-in 71-joint animation clips matching `af_clip` in `afcore.h`.
 */
enum class AnimationClipPreset(val nativeId: Int, val label: String) {
    REST(0, "Rest"),
    IDLE(1, "Idle"),
    BREATHE(2, "Breathe"),
    WALK(3, "Walk"),
    RUN(4, "Run"),
    JOG(5, "Jog"),
    SQUAT(6, "Squat"),
    JUMP(7, "Jump"),
    WAVE(8, "Wave"),
    PUNCH(9, "Punch"),
    KICK(10, "Kick"),
    T_POSE(11, "T-Pose"),
    A_POSE(12, "A-Pose"),
    SIT(13, "Sit"),
    TURN_360(14, "Turn 360"),
    PUSHUP(15, "Pushup"),
}

/**
 * Live 52-ARKit facial expression presets matching `nativeApplyExpression` in `avatarforge_renderer_jni.cpp`.
 */
enum class FacialExpressionPreset(val nativeId: Int, val label: String) {
    NEUTRAL(0, "Neutral"),
    SMILE(1, "Smile"),
    BLINK(2, "Blink"),
    WINK(3, "Wink"),
    SURPRISED(4, "Surprised"),
    FROWN(5, "Frown"),
    GAZE_LEFT(6, "Gaze Left"),
    GAZE_RIGHT(7, "Gaze Right"),
}

/**
 * Human-readable labels for the 16 `af_region` IDs in `af_mesh.h` returned by tap-to-inspect picking.
 */
object AvatarRegionNames {
    fun displayName(regionId: Int): String = when (regionId) {
        0 -> "Skin"
        1 -> "Left Eye"
        2 -> "Right Eye"
        3 -> "Teeth"
        4 -> "Tongue"
        5 -> "Lips"
        6 -> "Hair Cap"
        7 -> "Brows & Lashes"
        8 -> "Nails"
        9 -> "Upper Garment"
        10 -> "Lower Garment"
        11 -> "Footwear"
        12 -> "Gear / Armor"
        13 -> "Left Cornea"
        14 -> "Right Cornea"
        15 -> "Hair Cards"
        else -> "Imported Mesh"
    }
}

/**
 * Structured avatar customization state for the AvatarForge Studio UI.
 * Combines explicit anthropometric / style controls with optional prompt notes
 * into a deterministic [AvatarSpec] and human-readable prompt summary.
 */
data class AvatarCustomizationState(
    val sex: Sex = Sex.FEMALE,
    val build: Build = Build.ATHLETIC,
    val hairStyle: HairStyle = HairStyle.MEDIUM,
    val outfit: Outfit = Outfit.JEANS_JACKET,
    val heightCm: Int = 175,
    val skinMelanin: Float = 0.35f,
    val seed: Int = 1337,
    val customNotes: String = "",
) {
    fun clampedHeightM(): Float = (heightCm.coerceIn(145, 215) / 100f)

    fun buildPrompt(): String {
        val parts = mutableListOf<String>()
        val sexWord = when (sex) {
            Sex.FEMALE -> "Adult woman"
            Sex.MALE -> "Adult man"
            Sex.UNSPECIFIED -> "Adult person"
        }
        val buildWord = when (build) {
            Build.SLIM -> "slim build"
            Build.AVERAGE -> "average build"
            Build.ATHLETIC -> "athletic build"
            Build.MUSCULAR -> "muscular build"
            Build.HEAVY -> "heavy build"
        }
        val hairWord = when (hairStyle) {
            HairStyle.NONE -> "bald"
            HairStyle.BUZZ -> "buzz cut hair"
            HairStyle.SHORT -> "short hair"
            HairStyle.MEDIUM -> "medium hair"
            HairStyle.LONG -> "long hair"
            HairStyle.PONYTAIL -> "ponytail hair"
            HairStyle.BUN -> "hair bun"
            HairStyle.BRAIDS -> "braided hair"
            HairStyle.AFRO -> "afro hair"
            HairStyle.MOHAWK -> "mohawk hair"
        }
        val outfitWord = when (outfit) {
            Outfit.NONE -> "base anatomy"
            Outfit.TSHIRT -> "t-shirt and trousers"
            Outfit.DRESS -> "dress"
            Outfit.SUIT -> "tailored suit"
            Outfit.JEANS_JACKET -> "jacket and jeans"
            Outfit.ARMOR_PLATE -> "plate armor"
            Outfit.ARMOR_LEATHER -> "leather armor"
            Outfit.SPACESUIT -> "spacesuit"
            Outfit.ATHLETIC -> "athletic wear"
            Outfit.COAT -> "long coat"
        }
        parts += sexWord
        parts += buildWord
        parts += "${(clampedHeightM() * 100f).roundToInt()} cm tall"
        parts += hairWord
        parts += outfitWord
        val extra = customNotes.trim()
        if (extra.isNotEmpty()) parts += extra
        return parts.joinToString(", ")
    }

    fun toAvatarSpec(): AvatarSpec {
        val prompt = buildPrompt()
        val clampedMelanin = skinMelanin.coerceIn(0.05f, 0.95f)
        val footwear = when (outfit) {
            Outfit.ARMOR_PLATE -> Footwear.SABATON
            Outfit.ARMOR_LEATHER, Outfit.SPACESUIT, Outfit.COAT -> Footwear.BOOT
            Outfit.TSHIRT, Outfit.JEANS_JACKET, Outfit.ATHLETIC -> Footwear.SNEAKER
            Outfit.DRESS, Outfit.SUIT -> Footwear.BOOT
            Outfit.NONE -> Footwear.NONE
        }
        return AvatarSpec(
            sex = sex,
            build = build,
            hairStyle = hairStyle,
            outfit = outfit,
            footwear = footwear,
            seed = seed.coerceAtLeast(1),
            heightM = clampedHeightM(),
            skinMelanin = clampedMelanin,
            prompt = prompt,
        )
    }

    companion object {
        val SELECTABLE_OUTFITS = listOf(
            Outfit.TSHIRT,
            Outfit.JEANS_JACKET,
            Outfit.DRESS,
            Outfit.SUIT,
            Outfit.ATHLETIC,
            Outfit.COAT,
            Outfit.ARMOR_PLATE,
            Outfit.ARMOR_LEATHER,
            Outfit.SPACESUIT,
        )

        fun displayName(hair: HairStyle): String = when (hair) {
            HairStyle.NONE -> "Bald"
            HairStyle.BUZZ -> "Buzz"
            HairStyle.SHORT -> "Short"
            HairStyle.MEDIUM -> "Medium"
            HairStyle.LONG -> "Long"
            HairStyle.PONYTAIL -> "Ponytail"
            HairStyle.BUN -> "Bun"
            HairStyle.BRAIDS -> "Braids"
            HairStyle.AFRO -> "Afro"
            HairStyle.MOHAWK -> "Mohawk"
        }

        fun displayName(outfit: Outfit): String = when (outfit) {
            Outfit.NONE -> "None"
            Outfit.TSHIRT -> "T-Shirt"
            Outfit.DRESS -> "Dress"
            Outfit.SUIT -> "Suit"
            Outfit.JEANS_JACKET -> "Jacket & Jeans"
            Outfit.ARMOR_PLATE -> "Plate Armor"
            Outfit.ARMOR_LEATHER -> "Leather Armor"
            Outfit.SPACESUIT -> "Spacesuit"
            Outfit.ATHLETIC -> "Athletic"
            Outfit.COAT -> "Coat"
        }

        fun displayName(build: Build): String = when (build) {
            Build.SLIM -> "Slim"
            Build.AVERAGE -> "Average"
            Build.ATHLETIC -> "Athletic"
            Build.MUSCULAR -> "Muscular"
            Build.HEAVY -> "Heavy"
        }

        fun displayName(sex: Sex): String = when (sex) {
            Sex.FEMALE -> "Female"
            Sex.MALE -> "Male"
            Sex.UNSPECIFIED -> "Auto"
        }
    }
}
