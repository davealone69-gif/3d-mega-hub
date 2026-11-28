package com.davemega.hub

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.disabled
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.util.Locale

/** Controls only the clips that the currently loaded Filament asset actually exposes. */
@Composable
internal fun CharacterAnimationControls(
    usingFilament: Boolean,
    playback: AnimationPlaybackState,
    onSelectClip: (Int) -> Unit,
    onPlayingChange: (Boolean) -> Unit,
    onStop: () -> Unit,
    onLoopChange: (Boolean) -> Unit,
    onSpeedChange: (Float) -> Unit,
    onSeek: (Float) -> Unit,
) {
    val clips = playback.clips
    val current = playback.currentClip?.takeIf { it.isPlayable }
    val controlsEnabled = usingFilament && current != null

    Surface(
        shape = RoundedCornerShape(14.dp),
        color = HubColors.SurfaceRaised,
        border = BorderStroke(1.dp, HubColors.Outline),
    ) {
        Column(Modifier.fillMaxWidth().padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("ANIMATION  /  MOTION", color = HubColors.Accent, style = MaterialTheme.typography.labelSmall, letterSpacing = 1.sp)
            Text("Animate your character", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            val message = when {
                !usingFilament -> "Switch to the Filament preview above to play this model's animation clips."
                clips == null -> "Loading the model or switching animations. If loading fails, check the renderer status below."
                clips.isEmpty() -> "This model has no animation clips. Generate a character or import an animated GLB to enable these buttons."
                current == null -> "This model has no playable animation clips."
                else -> "${if (playback.playing) "Playing" else "Paused"}: ${current.label} · ${current.index + 1} / ${clips.size}"
            }
            Text(message, style = MaterialTheme.typography.bodySmall, color = HubColors.Muted)

            // Keep the common actions visible even for a static sample, but never invent clips.
            AnimationShortcut.entries.chunked(2).forEach { shortcuts ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    shortcuts.forEach { shortcut ->
                        val clip = if (usingFilament) playback.shortcut(shortcut) else null
                        val selected = clip != null && clip.index == playback.selectedIndex
                        val modifier = Modifier.weight(1f).semantics {
                            contentDescription = "${shortcut.label} animation"
                            // The description can be exposed as a separate accessibility node;
                            // mirror the disabled state there as well as on the Material button.
                            if (clip == null) disabled()
                        }
                        if (selected) {
                            Button(onClick = { clip?.let { onSelectClip(it.index) } }, enabled = clip != null, modifier = modifier) {
                                Text(shortcut.label)
                            }
                        } else {
                            OutlinedButton(onClick = { clip?.let { onSelectClip(it.index) } }, enabled = clip != null, modifier = modifier) {
                                Text(shortcut.label)
                            }
                        }
                    }
                }
            }

            if (usingFilament && !clips.isNullOrEmpty()) {
                if (AnimationShortcut.entries.any { playback.shortcut(it) == null }) {
                    Text("Shortcuts are enabled only when this model contains a matching clip. Other clips are below.", style = MaterialTheme.typography.bodySmall, color = HubColors.Muted)
                }
                Text("ALL CLIPS · SWIPE", style = MaterialTheme.typography.labelSmall, color = HubColors.Muted, letterSpacing = 0.7.sp)
                LazyRow(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(clips, key = { it.index }) { clip ->
                        FilterChip(
                            selected = clip.index == playback.selectedIndex && clip.isPlayable,
                            enabled = clip.isPlayable,
                            onClick = { onSelectClip(clip.index) },
                            label = { Text(clip.label, maxLines = 1, overflow = TextOverflow.Ellipsis) },
                        )
                    }
                }
                if (clips.any { !it.isPlayable }) {
                    Text("Clips without a playable duration are disabled.", style = MaterialTheme.typography.bodySmall, color = HubColors.Muted)
                }
            }

            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { onPlayingChange(!playback.playing) }, enabled = controlsEnabled, modifier = Modifier.weight(1f)) {
                    Text(if (playback.playing && controlsEnabled) "Pause" else "Play")
                }
                OutlinedButton(onClick = onStop, enabled = controlsEnabled, modifier = Modifier.weight(1f)) {
                    Text("Stop / reset")
                }
            }

            if (controlsEnabled && current != null) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(
                        onClick = { playback.previousClip?.let { onSelectClip(it.index) } },
                        enabled = playback.previousClip != null,
                        modifier = Modifier.weight(1f),
                    ) { Text("Previous") }
                    OutlinedButton(
                        onClick = { playback.nextClip?.let { onSelectClip(it.index) } },
                        enabled = playback.nextClip != null,
                        modifier = Modifier.weight(1f),
                    ) { Text("Next") }
                }
                val position = playback.positionSeconds.takeIf { it.isFinite() }
                    ?.coerceIn(0f, current.durationSeconds) ?: 0f
                Slider(
                    value = position,
                    onValueChange = onSeek,
                    valueRange = 0f..current.durationSeconds,
                    modifier = Modifier.semantics { contentDescription = "Animation position" },
                )
                Text(
                    String.format(Locale.US, "%.2f / %.2f seconds", position, current.durationSeconds),
                    style = MaterialTheme.typography.bodySmall,
                    color = HubColors.Muted,
                )
                OutlinedButton(onClick = { onLoopChange(!playback.looping) }) {
                    Text(if (playback.looping) "Loop: on" else "Loop: off")
                }
                Text("SPEED", style = MaterialTheme.typography.labelSmall, color = HubColors.Muted, letterSpacing = 1.sp)
                LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(listOf(0.5f, 1f, 1.5f, 2f)) { speed ->
                        FilterChip(
                            selected = speed == playback.speed,
                            onClick = { onSpeedChange(speed) },
                            label = { Text(if (speed == 1f) "1×" else "${speed}×") },
                        )
                    }
                }
            }
        }
    }
}
