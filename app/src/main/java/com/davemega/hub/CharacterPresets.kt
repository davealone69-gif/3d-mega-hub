package com.davemega.hub

import com.davemega.hub.avatarforge.AvatarSpec
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import kotlin.math.roundToInt

/** A reproducible starting point for the on-device generator, not a bundled 3D model. */
internal data class CharacterPreset(
    val id: String,
    val name: String,
    val category: String,
    val description: String,
    val prompt: String,
    private val makeSpec: () -> AvatarSpec,
) {
    // Make a new spec (including fresh colour arrays) for each generation.
    fun createSpec(): AvatarSpec = makeSpec().copy(prompt = prompt)

    fun initialDraft(): CharacterDraft {
        val spec = createSpec()
        return CharacterDraft(
            presetId = id,
            sex = spec.sex,
            build = spec.build,
            hair = spec.hairStyle,
            outfit = spec.outfit,
            footwear = spec.footwear,
            heightCm = (spec.heightM * 100f).roundToInt(),
            skin = SkinChoice.MEDIUM,
            hairColor = HairColorChoice.DARK,
        )
    }
}

internal enum class SkinChoice(val label: String, val melanin: Float, val swatch: Long) {
    LIGHT("Light", 0.12f, 0xFFF0CBB6),
    MEDIUM("Medium", 0.35f, 0xFFC88E70),
    TAN("Tan", 0.52f, 0xFFA96E4E),
    DEEP("Deep", 0.75f, 0xFF68402E),
}

internal enum class HairColorChoice(val label: String, val red: Float, val green: Float, val blue: Float, val swatch: Long) {
    DARK("Dark", 0.045f, 0.035f, 0.030f, 0xFF292222),
    BROWN("Brown", 0.16f, 0.085f, 0.045f, 0xFF79523B),
    BLONDE("Blonde", 0.46f, 0.34f, 0.19f, 0xFFC0A278),
    SILVER("Silver", 0.56f, 0.57f, 0.58f, 0xFFB5B8BC),
}

/** Editable parameters with a prompt that cannot contradict the explicit native spec. */
internal data class CharacterDraft(
    val presetId: String,
    val sex: Sex,
    val build: Build,
    val hair: HairStyle,
    val outfit: Outfit,
    val footwear: Footwear,
    val heightCm: Int,
    val skin: SkinChoice,
    val hairColor: HairColorChoice,
) {
    init {
        require(sex in CharacterPresets.sexOptions)
        require(build in CharacterPresets.buildOptions)
        require(hair in CharacterPresets.hairOptions)
        require(outfit in CharacterPresets.outfitOptions)
        require(footwear in CharacterPresets.footwearOptions)
        require(heightCm in 140..220)
    }

    fun prompt(): String {
        val person = if (sex == Sex.FEMALE) "woman" else "man"
        val buildWord = when (build) {
            Build.SLIM -> "slim"
            Build.ATHLETIC -> "athletic"
            Build.MUSCULAR -> "muscular"
            Build.HEAVY -> "heavy"
            else -> "average"
        }
        val hairWords = when (hair) {
            HairStyle.NONE -> "bald"
            HairStyle.SHORT -> "short hair"
            HairStyle.MEDIUM -> "medium hair"
            HairStyle.LONG -> "long hair"
            HairStyle.PONYTAIL -> "ponytail"
            HairStyle.BUN -> "bun"
            HairStyle.BRAIDS -> "braids"
            else -> error("Unsupported hair option")
        }
        val outfitWords = when (outfit) {
            Outfit.ARMOR_LEATHER -> "leather armor"
            Outfit.ARMOR_PLATE -> "plate armor"
            Outfit.JEANS_JACKET -> "jacket and jeans"
            Outfit.SPACESUIT -> "spacesuit"
            Outfit.TSHIRT -> "t-shirt"
            else -> error("Unsupported outfit option")
        }
        val shoes = if (footwear == Footwear.BOOT) "boots" else "sneakers"
        return "Adult $person, $buildWord build, $heightCm cm tall, $hairWords, $outfitWords and $shoes"
    }

    fun createSpec(preset: CharacterPreset): AvatarSpec {
        require(preset.id == presetId)
        val muscle = when (build) {
            Build.SLIM -> 0.35f
            Build.ATHLETIC -> 0.60f
            Build.MUSCULAR -> 0.85f
            else -> 0.45f
        }
        val bodyFat = when (build) {
            Build.SLIM -> 0.14f
            Build.ATHLETIC -> 0.18f
            Build.MUSCULAR -> 0.16f
            Build.HEAVY -> 0.42f
            else -> 0.22f
        }
        return preset.createSpec().copy(
            sex = sex,
            build = build,
            hairStyle = hair,
            outfit = outfit,
            footwear = footwear,
            heightM = heightCm / 100f,
            muscle = muscle,
            bodyFat = bodyFat,
            skinMelanin = skin.melanin,
            hairTone = floatArrayOf(hairColor.red, hairColor.green, hairColor.blue),
            prompt = prompt(),
            outfitExplicit = true,
        )
    }
}

internal object CharacterPresets {
    const val DEFAULT_PROMPT = "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans"

    val sexOptions = listOf(Sex.FEMALE, Sex.MALE)
    val buildOptions = listOf(Build.AVERAGE, Build.SLIM, Build.ATHLETIC, Build.MUSCULAR, Build.HEAVY)
    val hairOptions = listOf(HairStyle.NONE, HairStyle.SHORT, HairStyle.MEDIUM, HairStyle.LONG, HairStyle.PONYTAIL, HairStyle.BUN, HairStyle.BRAIDS)
    // The editor deliberately excludes the engine's unclothed anatomical base.
    val outfitOptions = listOf(Outfit.ARMOR_LEATHER, Outfit.ARMOR_PLATE, Outfit.JEANS_JACKET, Outfit.SPACESUIT, Outfit.TSHIRT)
    val footwearOptions = listOf(Footwear.BOOT, Footwear.SNEAKER)

    val SCOUT = CharacterPreset(
        id = "scout",
        name = "Scout",
        category = "RECON / 01",
        description = "Leather armor · ponytail · boots",
        prompt = "Adult woman, athletic build, 175 cm tall, ponytail, leather armor and boots",
        makeSpec = {
            AvatarSpec(
                sex = Sex.FEMALE,
                build = Build.ATHLETIC,
                hairStyle = HairStyle.PONYTAIL,
                outfit = Outfit.ARMOR_LEATHER,
                footwear = Footwear.BOOT,
                seed = 4101,
                heightM = 1.75f,
                ageYears = 30f,
                muscle = 0.60f,
                bodyFat = 0.18f,
                hairTone = floatArrayOf(0.045f, 0.035f, 0.030f),
                clothTone = floatArrayOf(0.16f, 0.17f, 0.12f),
                gearTone = floatArrayOf(0.16f, 0.10f, 0.06f),
                outfitExplicit = true,
            )
        },
    )

    val SENTINEL = CharacterPreset(
        id = "sentinel",
        name = "Sentinel",
        category = "ARMORED / 02",
        description = "Plate armor · short hair · boots",
        prompt = "Adult man, athletic build, 182 cm tall, short hair, plate armor and boots",
        makeSpec = {
            AvatarSpec(
                sex = Sex.MALE,
                build = Build.ATHLETIC,
                hairStyle = HairStyle.SHORT,
                outfit = Outfit.ARMOR_PLATE,
                footwear = Footwear.BOOT,
                seed = 4102,
                heightM = 1.82f,
                ageYears = 32f,
                muscle = 0.60f,
                bodyFat = 0.18f,
                hairTone = floatArrayOf(0.045f, 0.035f, 0.030f),
                clothTone = floatArrayOf(0.09f, 0.10f, 0.10f),
                gearTone = floatArrayOf(0.12f, 0.13f, 0.14f),
                outfitExplicit = true,
            )
        },
    )

    val all: List<CharacterPreset> = listOf(SCOUT, SENTINEL)

    fun find(id: String?): CharacterPreset? = all.firstOrNull { it.id == id }

    /** Never apply a preset's hidden parameters to an edited, mismatched or stale prompt. */
    fun specForDraft(id: String?, prompt: String, draft: CharacterDraft?): AvatarSpec {
        val trimmedPrompt = prompt.trim()
        val preset = find(id)
        return if (preset != null && draft != null && draft.presetId == id && draft.prompt() == trimmedPrompt) {
            draft.createSpec(preset)
        } else {
            AvatarSpec(prompt = trimmedPrompt)
        }
    }
}
