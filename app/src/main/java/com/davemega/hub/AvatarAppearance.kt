package com.davemega.hub

import com.davemega.hub.avatarforge.AvatarSpec
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex

/**
 * Hair pigment presets, in linear sRGB to match [AvatarSpec.hairTone].
 *
 * [NATURAL] is all zero on purpose: the engine treats a zero triplet as "derive the hair colour
 * from skin melanin", which is the documented default and the only option that stays consistent
 * with a prompt that never mentioned hair colour.
 */
enum class HairColour(val label: String, private val linearRgb: FloatArray) {
    NATURAL("Natural", floatArrayOf(0f, 0f, 0f)),
    BLACK("Black", floatArrayOf(0.014f, 0.012f, 0.011f)),
    DARK_BROWN("Dark brown", floatArrayOf(0.032f, 0.016f, 0.008f)),
    BROWN("Brown", floatArrayOf(0.062f, 0.032f, 0.014f)),
    AUBURN("Auburn", floatArrayOf(0.090f, 0.026f, 0.012f)),
    BLONDE("Blonde", floatArrayOf(0.320f, 0.230f, 0.110f)),
    SILVER("Silver", floatArrayOf(0.380f, 0.380f, 0.395f));

    fun toLinearRgb(): FloatArray = linearRgb.copyOf()
}

/** Primary garment colour presets, linear sRGB; [NEUTRAL] defers to the engine's grey default. */
enum class ClothColour(val label: String, private val linearRgb: FloatArray) {
    NEUTRAL("Neutral", floatArrayOf(0f, 0f, 0f)),
    INK("Ink", floatArrayOf(0.020f, 0.022f, 0.028f)),
    CRIMSON("Crimson", floatArrayOf(0.170f, 0.020f, 0.030f)),
    FOREST("Forest", floatArrayOf(0.018f, 0.062f, 0.032f)),
    SAND("Sand", floatArrayOf(0.240f, 0.200f, 0.140f)),
    WHITE("White", floatArrayOf(0.700f, 0.700f, 0.710f));

    fun toLinearRgb(): FloatArray = linearRgb.copyOf()
}

/**
 * The user-facing appearance state of Avatar Studio.
 *
 * This is the piece the app was missing: [AvatarSpec] has carried hair style, outfit, melanin and
 * three colour triplets since the beginning, but the studio only ever built a spec from prompt text,
 * so skin tone and hair changes could not be requested from the UI at all. [AvatarAppearance] is the
 * editable subset, [toSpec] is the single translation into the engine's spec, and [describe] is the
 * one-line summary the status area shows so the user can see exactly what will be generated.
 *
 * Deliberately absent: `Outfit.NONE`. That value is the engine's unclothed adult anatomical surface
 * and requires the explicit adult-mode confirmation gate, so it is not reachable from these controls.
 */
data class AvatarAppearance(
    val sex: Sex = Sex.UNSPECIFIED,
    val build: Build = Build.AVERAGE,
    val hairStyle: HairStyle = HairStyle.MEDIUM,
    val outfit: Outfit = Outfit.JEANS_JACKET,
    val hairColour: HairColour = HairColour.NATURAL,
    val clothColour: ClothColour = ClothColour.NEUTRAL,
    val skinMelanin: Float = 0.35f,
    val heightCm: Float = 175f,
    val ageYears: Float = 30f
) {
    /** Values clamped into the ranges the engine and the anthropometry solver accept. */
    fun normalized(): AvatarAppearance = copy(
        skinMelanin = skinMelanin.finiteOr(0.35f).coerceIn(0f, 1f),
        heightCm = heightCm.finiteOr(175f)
            .coerceIn(AvatarSpec.MIN_HEIGHT_M * 100f, AvatarSpec.MAX_HEIGHT_M * 100f),
        ageYears = ageYears.finiteOr(30f).coerceIn(18f, 70f)
    )

    /**
     * Builds the engine spec. [AvatarSpec.requireGeneratable] runs here as well, so selecting an
     * unclothed outfit without adult-mode confirmation fails at the UI boundary rather than deep in
     * the native pipeline.
     */
    fun toSpec(
        prompt: String = "",
        seed: Int = DEFAULT_SEED,
        adultModeConfirmed: Boolean = false
    ): AvatarSpec {
        val safe = normalized()
        return AvatarSpec(
            sex = safe.sex,
            build = safe.build,
            hairStyle = safe.hairStyle,
            outfit = safe.outfit,
            seed = seed,
            heightM = safe.heightCm * 0.01f,
            ageYears = safe.ageYears,
            skinMelanin = safe.skinMelanin,
            hairTone = safe.hairColour.toLinearRgb(),
            clothTone = safe.clothColour.toLinearRgb(),
            prompt = prompt,
            outfitExplicit = safe.outfit != Outfit.NONE,
            adultModeConfirmed = adultModeConfirmed
        ).requireGeneratable(prompt)
    }

    /** One-line summary for the studio status area. */
    fun describe(): String {
        val safe = normalized()
        return buildString {
            append(safe.sex.name.lowercase().replace('_', ' '))
            append(" · ")
            append(safe.build.name.lowercase().replace('_', ' '))
            append(" · ")
            append("${safe.hairStyle.name.lowercase()} hair")
            append(" (${safe.hairColour.label.lowercase()})")
            append(" · ")
            append(safe.outfit.name.lowercase().replace('_', ' '))
            append(" · ")
            append(safe.clothColour.label.lowercase())
            append(" cloth")
            append(" · melanin ${"%.2f".format(safe.skinMelanin)}")
            append(" · ${"%.0f".format(safe.heightCm)} cm")
        }
    }

    companion object {
        const val DEFAULT_SEED = 1337

        /** Outfits offered by the controls. `Outfit.NONE` is excluded: it needs the adult gate. */
        val selectableOutfits: List<Outfit> = Outfit.entries.filter { it != Outfit.NONE }
    }
}

internal fun Float.finiteOr(fallback: Float): Float = if (isFinite()) this else fallback

/**
 * Persistence encoding for [AvatarAppearance], split out of Compose so it can be unit-tested on the
 * JVM. Values are stored by enum name, and anything unreadable falls back to the default rather than
 * failing the screen.
 */
object AppearanceStore {
    private const val KEY_SEX = "appearance_sex"
    private const val KEY_BUILD = "appearance_build"
    private const val KEY_HAIR_STYLE = "appearance_hair_style"
    private const val KEY_OUTFIT = "appearance_outfit"
    private const val KEY_HAIR_COLOUR = "appearance_hair_colour"
    private const val KEY_CLOTH_COLOUR = "appearance_cloth_colour"
    private const val KEY_MELANIN = "appearance_melanin"
    private const val KEY_HEIGHT_CM = "appearance_height_cm"
    private const val KEY_AGE = "appearance_age"

    fun encode(appearance: AvatarAppearance): Map<String, String> {
        val safe = appearance.normalized()
        return mapOf(
            KEY_SEX to safe.sex.name,
            KEY_BUILD to safe.build.name,
            KEY_HAIR_STYLE to safe.hairStyle.name,
            KEY_OUTFIT to safe.outfit.name,
            KEY_HAIR_COLOUR to safe.hairColour.name,
            KEY_CLOTH_COLOUR to safe.clothColour.name,
            KEY_MELANIN to safe.skinMelanin.toString(),
            KEY_HEIGHT_CM to safe.heightCm.toString(),
            KEY_AGE to safe.ageYears.toString()
        )
    }

    fun decode(read: (String) -> String?): AvatarAppearance {
        val defaults = AvatarAppearance()
        fun <T : Enum<T>> enumOr(key: String, values: Array<T>, fallback: T): T {
            val stored = read(key) ?: return fallback
            return values.firstOrNull { it.name == stored } ?: fallback
        }
        fun floatOr(key: String, fallback: Float): Float =
            read(key)?.toFloatOrNull()?.takeIf { it.isFinite() } ?: fallback

        return AvatarAppearance(
            sex = enumOr(KEY_SEX, Sex.entries.toTypedArray(), defaults.sex),
            build = enumOr(KEY_BUILD, Build.entries.toTypedArray(), defaults.build),
            hairStyle = enumOr(KEY_HAIR_STYLE, HairStyle.entries.toTypedArray(), defaults.hairStyle),
            outfit = enumOr(KEY_OUTFIT, Outfit.entries.toTypedArray(), defaults.outfit),
            hairColour = enumOr(KEY_HAIR_COLOUR, HairColour.entries.toTypedArray(), defaults.hairColour),
            clothColour = enumOr(KEY_CLOTH_COLOUR, ClothColour.entries.toTypedArray(), defaults.clothColour),
            skinMelanin = floatOr(KEY_MELANIN, defaults.skinMelanin),
            heightCm = floatOr(KEY_HEIGHT_CM, defaults.heightCm),
            ageYears = floatOr(KEY_AGE, defaults.ageYears)
        ).normalized()
    }
}
