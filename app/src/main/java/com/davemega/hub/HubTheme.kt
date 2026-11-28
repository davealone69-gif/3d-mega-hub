package com.davemega.hub

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

internal object HubColors {
    val Background = Color(0xFF101318)
    val Surface = Color(0xFF1A1E25)
    val SurfaceRaised = Color(0xFF292E37)
    val Outline = Color(0xFF4B505B)
    val Accent = Color(0xFFE4A4BB)
    val AccentMuted = Color(0xFFBE8FA2)
    val Secondary = Color(0xFFB8C5CF)
    val Text = Color(0xFFF3F0F1)
    val Muted = Color(0xFFB9BFC9)
}

private val HubColorScheme = darkColorScheme(
    primary = HubColors.Accent,
    onPrimary = Color(0xFF371B29),
    primaryContainer = Color(0xFF593446),
    onPrimaryContainer = Color(0xFFFFD9E5),
    secondary = HubColors.Secondary,
    onSecondary = Color(0xFF22303A),
    secondaryContainer = Color(0xFF374650),
    onSecondaryContainer = Color(0xFFD5E5EE),
    background = HubColors.Background,
    onBackground = HubColors.Text,
    surface = HubColors.Surface,
    onSurface = HubColors.Text,
    surfaceVariant = HubColors.SurfaceRaised,
    onSurfaceVariant = HubColors.Muted,
    outline = HubColors.Outline,
)

private val HubShapes = Shapes(
    small = RoundedCornerShape(8.dp),
    medium = RoundedCornerShape(14.dp),
    large = RoundedCornerShape(20.dp),
    extraLarge = RoundedCornerShape(24.dp),
)

@Composable
internal fun MegaHubTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = HubColorScheme, shapes = HubShapes, content = content)
}
