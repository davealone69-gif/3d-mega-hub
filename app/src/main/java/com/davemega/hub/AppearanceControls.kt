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
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex

/**
 * The appearance presets the AvatarForge engine already has, as things the user can pick.
 *
 * Nothing here is new engine capability. Every option is an existing field of [AvatarSpec] that
 * until now could only be reached by typing the right word into the prompt box. The values go to
 * the engine as an explicit spec; the prompt sent with them names no hair, outfit or build, so the
 * prompt parser has nothing to override.
 */
object AppearanceCatalogue {
    /** Sent as the mandatory prompt text for a preset-driven build. It contains no preset keyword. */
    const val PRESET_PROMPT = "Avatar from the appearance controls"

    /** A colour as linear RGB, the form [AvatarSpec] takes. All zero means "engine default". */
    class Colour(val label: String, val red: Float, val green: Float, val blue: Float) {
        fun rgb(): FloatArray = floatArrayOf(red, green, blue)
    }

    val hairColours: List<Colour> = listOf(
        Colour("Engine default", 0f, 0f, 0f),
        Colour("Black", 0.012f, 0.011f, 0.010f),
        Colour("Dark brown", 0.045f, 0.026f, 0.014f),
        Colour("Brown", 0.130f, 0.070f, 0.032f),
        Colour("Blonde", 0.520f, 0.360f, 0.150f),
        Colour("Red", 0.260f, 0.050f, 0.022f),
        Colour("Grey", 0.300f, 0.300f, 0.300f),
        Colour("White", 0.780f, 0.780f, 0.760f)
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

/** What the appearance controls hold. Turned into an [AvatarSpec] when the user generates. */
data class AppearanceSelection(
    val sex: Sex = Sex.FEMALE,
    val build: Build = Build.AVERAGE,
    val hairStyle: HairStyle = HairStyle.MEDIUM,
    val outfit: Outfit = Outfit.TSHIRT,
    val footwear: Footwear = Footwear.SNEAKER,
    val heightM: Float = 1.70f,
    val skinMelanin: Float = 0.35f,
    val hairColour: Int = 0,
    val clothColour: Int = 0
) {
    fun toSpec(): AvatarSpec = AvatarSpec(
        sex = sex,
        build = build,
        hairStyle = hairStyle,
        outfit = outfit,
        footwear = footwear,
        heightM = heightM.coerceIn(AppearanceCatalogue.MIN_HEIGHT_M, AppearanceCatalogue.MAX_HEIGHT_M),
        skinMelanin = skinMelanin.coerceIn(0f, 1f),
        hairTone = AppearanceCatalogue.hairColours[hairColour.coerceIn(0, AppearanceCatalogue.hairColours.size - 1)].rgb(),
        clothTone = AppearanceCatalogue.clothColours[clothColour.coerceIn(0, AppearanceCatalogue.clothColours.size - 1)].rgb(),
        prompt = AppearanceCatalogue.PRESET_PROMPT,
        outfitExplicit = true
    )

    /** One line for the status text, so the user can see what was asked for. */
    fun summary(): String =
        "${AppearanceCatalogue.label(sex.name)}, ${AppearanceCatalogue.label(build.name).lowercase()} build, " +
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
        ChoiceRow("Build", Build.entries.map { AppearanceCatalogue.label(it.name) }, Build.entries.indexOf(selection.build), enabled) {
            onChange(selection.copy(build = Build.entries[it]))
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
