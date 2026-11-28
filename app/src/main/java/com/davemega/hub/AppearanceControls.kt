package com.davemega.hub

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.davemega.hub.avatarforge.AvatarSpec
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.FaceParam
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex

/**
 * The appearance presets the AvatarForge engine already has, as things the user can pick.
 *
 * Every option is a field of [AvatarSpec]. The values go to the engine as an explicit spec; the
 * prompt sent with them names no hair, outfit or build, so the prompt parser has nothing to
 * override.
 */
object AppearanceCatalogue {
    /** Sent as the mandatory prompt text for a preset-driven build. It contains no preset keyword. */
    const val PRESET_PROMPT = "Avatar from the appearance controls"

    /** A colour as linear RGB, the form [AvatarSpec] takes. All zero means "engine default". */
    class Colour(val label: String, val red: Float, val green: Float, val blue: Float) {
        fun rgb(): FloatArray = floatArrayOf(red, green, blue)
    }

    /** Index 0 is the engine's own choice. Brown, black, blonde, ginger and pink lead, as in the owner's reference. */
    val hairColours: List<Colour> = listOf(
        Colour("Auto", 0f, 0f, 0f),
        Colour("Brown", 0.130f, 0.070f, 0.032f),
        Colour("Black", 0.012f, 0.011f, 0.010f),
        Colour("Blonde", 0.560f, 0.400f, 0.170f),
        Colour("Ginger", 0.420f, 0.110f, 0.020f),
        Colour("Pink", 0.800f, 0.130f, 0.330f),
        Colour("Dark brown", 0.045f, 0.026f, 0.014f),
        Colour("Red", 0.260f, 0.030f, 0.022f),
        Colour("Grey", 0.300f, 0.300f, 0.300f),
        Colour("White", 0.780f, 0.780f, 0.760f),
        Colour("Blue", 0.030f, 0.120f, 0.520f),
        Colour("Purple", 0.200f, 0.040f, 0.420f)
    )

    /** Iris colours. Index 0 is the engine's own choice, which follows the skin tone. */
    val eyeColours: List<Colour> = listOf(
        Colour("Auto", 0f, 0f, 0f),
        Colour("Dark brown", 0.110f, 0.050f, 0.020f),
        Colour("Brown", 0.230f, 0.110f, 0.040f),
        Colour("Hazel", 0.300f, 0.230f, 0.080f),
        Colour("Green", 0.130f, 0.300f, 0.130f),
        Colour("Blue", 0.150f, 0.300f, 0.560f),
        Colour("Grey", 0.330f, 0.380f, 0.420f)
    )

    val clothColours: List<Colour> = listOf(
        Colour("Engine default", 0f, 0f, 0f),
        Colour("Black", 0.012f, 0.012f, 0.014f),
        Colour("White", 0.800f, 0.800f, 0.800f),
        Colour("Navy", 0.010f, 0.020f, 0.120f),
        Colour("Red", 0.450f, 0.020f, 0.020f),
        Colour("Green", 0.030f, 0.200f, 0.060f),
        Colour("Tan", 0.420f, 0.290f, 0.160f)
    )

    /** The unclothed base ([Outfit.NONE]) needs adult-mode confirmation and is not offered here. */
    val outfits: List<Outfit> = Outfit.entries.filter { it != Outfit.NONE }

    const val MIN_HEIGHT_M = 1.45f
    const val MAX_HEIGHT_M = 2.05f

    fun label(enumName: String): String =
        enumName.lowercase().replace('_', ' ').replaceFirstChar { it.uppercase() }
}

/**
 * Body type, as the creator offers it: the engine's build (a table of girths) together with the
 * muscle and body-fat values that go with the name. The first five are the five of the owner's
 * reference, in its order; "Plus size" is its "BBW".
 */
enum class BodyType(val label: String, val build: Build, val muscle: Float, val bodyFat: Float) {
    SLIM("Slim", Build.SLIM, 0.38f, 0.16f),
    ATHLETIC("Athletic", Build.ATHLETIC, 0.64f, 0.16f),
    AVERAGE("Average", Build.AVERAGE, 0.45f, 0.22f),
    CURVY("Curvy", Build.CURVY, 0.42f, 0.27f),
    PLUS_SIZE("Plus size", Build.HEAVY, 0.40f, 0.36f),
    MUSCULAR("Muscular", Build.MUSCULAR, 0.85f, 0.15f)
}

/** Breast size, female bodies only: [AvatarSpec.bust]. */
enum class BustSize(val label: String, val bust: Float) {
    FLAT("Flat", 0.04f), SMALL("Small", 0.24f), AVERAGE("Average", 0.45f), BIG("Big", 0.70f), HUGE("Huge", 1.00f)
}

/** Butt size: [AvatarSpec.glute], an offset about the body type's own. */
enum class GluteSize(val label: String, val glute: Float) {
    SMALL("Small", -0.90f), SLIM("Slim", -0.45f), ATHLETIC("Athletic", 0.0f), BIG("Big", 0.50f), HUGE("Huge", 1.00f)
}

/**
 * A starting point for the look: a skin tone, a hair and an eye colour, and face proportions.
 *
 * These are the six groups of the owner's reference. Each is one common combination chosen for
 * the name, not a rule about anybody: every value can be changed afterwards in Skin, Hair and
 * Face, and no option of the creator is closed to any of them.
 *
 * [face] is indexed by [FaceParam]: nose width, nose bridge, lip fullness, eye tilt, eye
 * openness, cheekbones, jaw width.
 */
enum class Ethnicity(
    val label: String,
    val melanin: Float,
    val hairColour: String,
    val eyeColour: String,
    private val faceValues: List<Float>
) {
    WHITE("White", 0.10f, "Brown", "Blue", listOf(-0.25f, 0.45f, -0.20f, 0.0f, 0.10f, 0.0f, 0.0f)),
    ASIAN("Asian", 0.20f, "Black", "Dark brown", listOf(0.15f, -0.50f, 0.0f, 0.35f, -0.35f, 0.40f, 0.10f)),
    BLACK("Black", 0.85f, "Black", "Dark brown", listOf(0.50f, -0.35f, 0.50f, 0.10f, 0.10f, 0.20f, 0.10f)),
    LATINA("Latina", 0.38f, "Dark brown", "Brown", listOf(0.15f, 0.10f, 0.30f, 0.15f, 0.10f, 0.15f, 0.0f)),
    SOUTH_ASIAN("South Asian", 0.55f, "Black", "Dark brown", listOf(0.10f, 0.30f, 0.20f, 0.05f, 0.25f, 0.0f, -0.10f)),
    MIDDLE_EASTERN("Middle Eastern", 0.33f, "Black", "Hazel", listOf(0.0f, 0.65f, 0.10f, 0.10f, 0.15f, 0.10f, 0.05f));

    /** The preset's face as [AppearanceSelection.face] holds it. */
    fun face(): List<Float> = List(AppearanceSelection.FACE_SLIDERS) { faceValues.getOrElse(it) { 0f } }

    /** "Latina" is the reference's word for a female avatar; a male one is "Latino". */
    fun labelFor(sex: Sex): String = if (this == LATINA && sex == Sex.MALE) "Latino" else label

    /** The selection with this preset applied: skin, hair colour, eye colour and face. */
    fun applyTo(selection: AppearanceSelection): AppearanceSelection = selection.copy(
        ethnicity = this,
        skinMelanin = melanin,
        hairColour = AppearanceCatalogue.hairColours.indexOfFirst { it.label == hairColour }.coerceAtLeast(0),
        eyeColour = AppearanceCatalogue.eyeColours.indexOfFirst { it.label == eyeColour }.coerceAtLeast(0),
        face = face()
    )
}

/** What the appearance controls hold. Turned into an [AvatarSpec] when the user generates. */
data class AppearanceSelection(
    val sex: Sex = Sex.FEMALE,
    val bodyType: BodyType = BodyType.AVERAGE,
    val hairStyle: HairStyle = HairStyle.MEDIUM,
    val outfit: Outfit = Outfit.TSHIRT,
    val footwear: Footwear = Footwear.SNEAKER,
    val heightM: Float = 1.70f,
    val skinMelanin: Float = 0.35f,
    val hairColour: Int = 0,
    val clothColour: Int = 0,
    /** The preset last applied, for the tick on its card. It carries no value of its own. */
    val ethnicity: Ethnicity? = null,
    val bust: BustSize = BustSize.AVERAGE,
    val glute: GluteSize = GluteSize.ATHLETIC,
    val eyeColour: Int = 0,
    /** Offsets from the default face, -1..+1, in the order of [FaceParam]. */
    val face: List<Float> = List(FACE_SLIDERS) { 0f }
) {
    /** The engine's build for the chosen body type. */
    val build: Build get() = bodyType.build

    fun toSpec(): AvatarSpec = AvatarSpec(
        sex = sex,
        build = bodyType.build,
        muscle = bodyType.muscle,
        bodyFat = bodyType.bodyFat,
        bust = bust.bust,
        glute = glute.glute,
        // The creator builds adults only. The engine ignores bust and glute below 18 in any case.
        ageYears = ADULT_AGE_YEARS,
        hairStyle = hairStyle,
        outfit = outfit,
        footwear = footwear,
        heightM = heightM.coerceIn(AppearanceCatalogue.MIN_HEIGHT_M, AppearanceCatalogue.MAX_HEIGHT_M),
        skinMelanin = skinMelanin.coerceIn(0f, 1f),
        hairTone = AppearanceCatalogue.hairColours[hairColour.coerceIn(0, AppearanceCatalogue.hairColours.size - 1)].rgb(),
        clothTone = AppearanceCatalogue.clothColours[clothColour.coerceIn(0, AppearanceCatalogue.clothColours.size - 1)].rgb(),
        eyeTone = AppearanceCatalogue.eyeColours[eyeColour.coerceIn(0, AppearanceCatalogue.eyeColours.size - 1)].rgb(),
        face = FloatArray(FaceParam.COUNT) { index -> face.getOrElse(index) { 0f }.coerceIn(-1f, 1f) },
        prompt = AppearanceCatalogue.PRESET_PROMPT,
        outfitExplicit = true
    )

    /** One face slider moved; the rest kept. The ethnicity tick stays: it marks the starting point. */
    fun withFace(param: FaceParam, value: Float): AppearanceSelection =
        copy(face = List(FACE_SLIDERS) { index -> if (index == param.index) value.coerceIn(-1f, 1f) else face.getOrElse(index) { 0f } })

    companion object {
        /** The face offsets the creator shows; the engine's last slot is reserved. */
        const val FACE_SLIDERS = 7

        /** Every avatar the creator builds is an adult of this age. */
        const val ADULT_AGE_YEARS = 30f
    }

    /** One line for the status text, so the user can see what was asked for. */
    fun summary(): String =
        "${AppearanceCatalogue.label(sex.name)}, ${bodyType.label.lowercase()} build, " +
            "${"%.2f".format(heightM)} m, hair ${AppearanceCatalogue.label(hairStyle.name).lowercase()}, " +
            "${AppearanceCatalogue.label(outfit.name).lowercase()}, footwear ${AppearanceCatalogue.label(footwear.name).lowercase()}"
}

@Composable
fun AppearanceControls(
    selection: AppearanceSelection,
    enabled: Boolean,
    onChange: (AppearanceSelection) -> Unit
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        ChoiceRow("Body", Sex.entries.map { AppearanceCatalogue.label(it.name) }, Sex.entries.indexOf(selection.sex), enabled) {
            onChange(selection.copy(sex = Sex.entries[it]))
        }
        ChoiceRow("Build", BodyType.entries.map { it.label }, BodyType.entries.indexOf(selection.bodyType), enabled) {
            onChange(selection.copy(bodyType = BodyType.entries[it]))
        }
        ChoiceRow("Hair style", HairStyle.entries.map { AppearanceCatalogue.label(it.name) }, HairStyle.entries.indexOf(selection.hairStyle), enabled) {
            onChange(selection.copy(hairStyle = HairStyle.entries[it]))
        }
        ChoiceRow("Hair colour", AppearanceCatalogue.hairColours.map { it.label }, selection.hairColour, enabled) {
            onChange(selection.copy(hairColour = it))
        }
        ChoiceRow("Clothing", AppearanceCatalogue.outfits.map { AppearanceCatalogue.label(it.name) }, AppearanceCatalogue.outfits.indexOf(selection.outfit), enabled) {
            onChange(selection.copy(outfit = AppearanceCatalogue.outfits[it]))
        }
        ChoiceRow("Clothing colour", AppearanceCatalogue.clothColours.map { it.label }, selection.clothColour, enabled) {
            onChange(selection.copy(clothColour = it))
        }
        ChoiceRow("Footwear", Footwear.entries.map { AppearanceCatalogue.label(it.name) }, Footwear.entries.indexOf(selection.footwear), enabled) {
            onChange(selection.copy(footwear = Footwear.entries[it]))
        }
        Text("Skin tone: ${"%.2f".format(selection.skinMelanin)} (0 lightest, 1 darkest)", style = MaterialTheme.typography.labelLarge)
        Slider(
            value = selection.skinMelanin.coerceIn(0f, 1f),
            onValueChange = { onChange(selection.copy(skinMelanin = it)) },
            valueRange = 0f..1f,
            enabled = enabled
        )
        Text("Height: ${"%.2f".format(selection.heightM)} m", style = MaterialTheme.typography.labelLarge)
        Slider(
            value = selection.heightM.coerceIn(AppearanceCatalogue.MIN_HEIGHT_M, AppearanceCatalogue.MAX_HEIGHT_M),
            onValueChange = { onChange(selection.copy(heightM = it)) },
            valueRange = AppearanceCatalogue.MIN_HEIGHT_M..AppearanceCatalogue.MAX_HEIGHT_M,
            enabled = enabled
        )
    }
}

@Composable
private fun ChoiceRow(
    title: String,
    options: List<String>,
    selected: Int,
    enabled: Boolean,
    onSelect: (Int) -> Unit
) {
    Text(title, style = MaterialTheme.typography.labelLarge)
    Row(
        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        options.forEachIndexed { index, option ->
            if (index == selected) {
                Button(onClick = { onSelect(index) }, enabled = enabled) { Text(option) }
            } else {
                OutlinedButton(onClick = { onSelect(index) }, enabled = enabled) { Text(option) }
            }
        }
    }
}
