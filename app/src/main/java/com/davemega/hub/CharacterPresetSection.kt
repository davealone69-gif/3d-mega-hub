package com.davemega.hub

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.Footwear
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import kotlin.math.roundToInt

@Composable
internal fun CharacterPresetSection(
    selected: CharacterPreset?,
    draft: CharacterDraft,
    canSelect: Boolean,
    canGenerate: Boolean,
    generating: Boolean,
    generationStatus: String,
    onSelect: (String) -> Unit,
    onDraftChange: (CharacterDraft) -> Unit,
    onGenerate: () -> Unit,
    onCancel: () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("CHARACTER LIBRARY  /  02", color = HubColors.Accent, style = MaterialTheme.typography.labelSmall, letterSpacing = 1.3.sp)
        Text("Choose a starting point", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
        Row(Modifier.fillMaxWidth().selectableGroup(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            CharacterPresets.all.forEach { preset ->
                val isSelected = selected?.id == preset.id
                Surface(
                    modifier = Modifier.weight(1f).selectable(
                        selected = isSelected,
                        enabled = canSelect,
                        role = Role.RadioButton,
                        onClick = { onSelect(preset.id) },
                    ).semantics { contentDescription = "${preset.name} preset" },
                    shape = RoundedCornerShape(16.dp),
                    color = if (isSelected) HubColors.SurfaceRaised else HubColors.Surface,
                    border = BorderStroke(1.dp, if (isSelected) HubColors.Accent else HubColors.Outline),
                ) {
                    Column {
                        Box(Modifier.fillMaxWidth().height(126.dp)) {
                            Image(
                                painter = painterResource(if (preset.id == CharacterPresets.SCOUT.id) R.drawable.scout_concept else R.drawable.sentinel_concept),
                                contentDescription = null,
                                modifier = Modifier.fillMaxSize(),
                                alignment = Alignment.TopCenter,
                                contentScale = ContentScale.Crop,
                            )
                            Box(Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color.Transparent, Color(0xE61A1E25)))))
                            Text(
                                "CONCEPT ART",
                                modifier = Modifier.align(Alignment.BottomStart).padding(10.dp),
                                color = Color.White,
                                style = MaterialTheme.typography.labelSmall,
                                letterSpacing = 0.7.sp,
                            )
                        }
                        Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                            Text(preset.category, color = HubColors.AccentMuted, style = MaterialTheme.typography.labelSmall, letterSpacing = 0.6.sp)
                            Text(preset.name, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                            Text(preset.description, style = MaterialTheme.typography.bodySmall, color = HubColors.Muted)
                            Text(if (isSelected) "SELECTED ✓" else "SELECT  →", color = HubColors.Accent, style = MaterialTheme.typography.labelSmall, fontWeight = FontWeight.SemiBold)
                        }
                    }
                }
            }
        }
        Text(
            "Concept art is illustrative. Generated characters use simplified armor; helmets, weapons and reference likenesses are not recreated.",
            style = MaterialTheme.typography.bodySmall,
            color = HubColors.Muted,
        )
        if (selected != null) {
            Button(onClick = onGenerate, enabled = canGenerate, modifier = Modifier.fillMaxWidth()) {
                Text(if (generating) "Generating…" else "Generate ${selected.name}")
            }
            if (generating) {
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    CircularProgressIndicator()
                    OutlinedButton(onClick = onCancel) { Text("Cancel generation") }
                }
            }
            Text(generationStatus, style = MaterialTheme.typography.bodySmall, color = HubColors.Muted)
            CharacterEditor(draft, canSelect, onDraftChange)
            Button(onClick = onGenerate, enabled = canGenerate, modifier = Modifier.fillMaxWidth()) {
                Text(if (generating) "Generating…" else "Generate with these settings")
            }
            if (generating) {
                OutlinedButton(onClick = onCancel, modifier = Modifier.fillMaxWidth()) { Text("Cancel generation") }
            }
        } else {
            Text("Select a preset to customize and generate it, or write your own description below the viewport.", style = MaterialTheme.typography.bodySmall, color = HubColors.Muted)
        }
    }
}

@Composable
private fun CharacterEditor(draft: CharacterDraft, enabled: Boolean, onChange: (CharacterDraft) -> Unit) {
    Surface(
        shape = RoundedCornerShape(16.dp),
        color = HubColors.Surface,
        border = BorderStroke(1.dp, HubColors.Outline.copy(alpha = 0.7f)),
    ) {
        Column(Modifier.fillMaxWidth().padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("CUSTOMIZE  /  PARAMETERS", color = HubColors.Accent, style = MaterialTheme.typography.labelSmall, letterSpacing = 1.sp)
            Text("Shape your character", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            Text(
                "Changes become a new 3D model when you generate. Keep the studio open while it runs.",
                style = MaterialTheme.typography.bodySmall,
                color = HubColors.Muted,
            )
            OptionRow("BODY", CharacterPresets.sexOptions, draft.sex, enabled, { if (it == Sex.FEMALE) "Female" else "Male" }) {
                onChange(draft.copy(sex = it))
            }
            OptionRow("BUILD", CharacterPresets.buildOptions, draft.build, enabled, { buildLabel(it) }) {
                onChange(draft.copy(build = it))
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("HEIGHT", style = MaterialTheme.typography.labelSmall, color = HubColors.Muted, letterSpacing = 1.sp)
                Text("${draft.heightCm} cm", style = MaterialTheme.typography.labelMedium, color = HubColors.Text)
            }
            Slider(
                value = draft.heightCm.toFloat(),
                onValueChange = { onChange(draft.copy(heightCm = it.roundToInt().coerceIn(140, 220))) },
                valueRange = 140f..220f,
                enabled = enabled,
            )
            OptionRow("HAIR STYLE", CharacterPresets.hairOptions, draft.hair, enabled, { hairLabel(it) }) {
                onChange(draft.copy(hair = it))
            }
            OptionRow("HAIR COLOR", HairColorChoice.entries, draft.hairColor, enabled, { it.label }, { Color(it.swatch) }) {
                onChange(draft.copy(hairColor = it))
            }
            OptionRow("SKIN TONE", SkinChoice.entries, draft.skin, enabled, { it.label }, { Color(it.swatch) }) {
                onChange(draft.copy(skin = it))
            }
            OptionRow("OUTFIT", CharacterPresets.outfitOptions, draft.outfit, enabled, { outfitLabel(it) }) {
                onChange(draft.copy(outfit = it))
            }
            OptionRow("FOOTWEAR", CharacterPresets.footwearOptions, draft.footwear, enabled, { if (it == Footwear.BOOT) "Boots" else "Sneakers" }) {
                onChange(draft.copy(footwear = it))
            }
        }
    }
}

@Composable
private fun <T> OptionRow(
    title: String,
    options: List<T>,
    selected: T,
    enabled: Boolean,
    label: (T) -> String,
    swatch: ((T) -> Color)? = null,
    onSelect: (T) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Text(title, style = MaterialTheme.typography.labelSmall, color = HubColors.Muted, letterSpacing = 1.sp)
        Row(
            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            options.forEach { option ->
                FilterChip(
                    selected = option == selected,
                    onClick = { onSelect(option) },
                    enabled = enabled,
                    label = { Text(label(option), maxLines = 1) },
                    leadingIcon = if (swatch == null) null else {
                        { Box(Modifier.size(14.dp).clip(CircleShape).background(swatch(option))) }
                    },
                )
            }
        }
    }
}

private fun buildLabel(build: Build): String = when (build) {
    Build.AVERAGE -> "Average"
    Build.SLIM -> "Slim"
    Build.ATHLETIC -> "Athletic"
    Build.MUSCULAR -> "Muscular"
    Build.HEAVY -> "Heavy"
}

private fun hairLabel(hair: HairStyle): String = when (hair) {
    HairStyle.NONE -> "Bald"
    HairStyle.SHORT -> "Short"
    HairStyle.MEDIUM -> "Medium"
    HairStyle.LONG -> "Long"
    HairStyle.PONYTAIL -> "Ponytail"
    HairStyle.BUN -> "Bun"
    HairStyle.BRAIDS -> "Braids"
    else -> hair.name.lowercase().replaceFirstChar { it.uppercase() }
}

private fun outfitLabel(outfit: Outfit): String = when (outfit) {
    Outfit.ARMOR_LEATHER -> "Leather armor"
    Outfit.ARMOR_PLATE -> "Plate armor"
    Outfit.JEANS_JACKET -> "Jacket & jeans"
    Outfit.SPACESUIT -> "Space suit"
    Outfit.TSHIRT -> "T-shirt"
    else -> outfit.name.lowercase()
}
