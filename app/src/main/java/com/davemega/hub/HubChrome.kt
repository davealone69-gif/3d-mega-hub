package com.davemega.hub

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

internal enum class HubPage(val label: String) {
    HOME("Home"), AVATAR("Avatar"), AI("AI"), VOICE("Voice"), SETTINGS("Settings")
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun HubAppBar() {
    TopAppBar(
        title = {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier.size(36.dp)
                        .background(HubColors.SurfaceRaised, RoundedCornerShape(10.dp))
                        .border(1.dp, HubColors.Outline, RoundedCornerShape(10.dp)),
                    contentAlignment = Alignment.Center,
                ) {
                    Text("M", color = HubColors.Accent, fontWeight = FontWeight.Black, fontSize = 20.sp)
                }
                Spacer(Modifier.width(10.dp))
                Column {
                    Text("3D MEGA HUB", fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.1.sp)
                    Text("CHARACTER STUDIO", color = HubColors.Muted, fontSize = 9.sp, letterSpacing = 1.5.sp)
                }
            }
        },
        actions = {
            Text(
                "CREATIVE LAB",
                modifier = Modifier.padding(end = 16.dp),
                color = HubColors.AccentMuted,
                style = MaterialTheme.typography.labelSmall,
                letterSpacing = 1.sp,
            )
        },
        colors = TopAppBarDefaults.topAppBarColors(containerColor = HubColors.Background),
    )
}

@Composable
internal fun HubBottomBar(page: HubPage, onPageSelected: (HubPage) -> Unit) {
    NavigationBar(containerColor = HubColors.Surface, contentColor = HubColors.Text) {
        HubPage.entries.forEach { item ->
            NavigationBarItem(
                selected = page == item,
                onClick = { onPageSelected(item) },
                icon = { HubIcon(item, Modifier.size(22.dp)) },
                label = { Text(item.label, fontSize = 10.sp, maxLines = 1) },
                colors = NavigationBarItemDefaults.colors(
                    selectedIconColor = HubColors.Accent,
                    selectedTextColor = HubColors.Accent,
                    indicatorColor = HubColors.SurfaceRaised,
                    unselectedIconColor = HubColors.Muted,
                    unselectedTextColor = HubColors.Muted,
                ),
            )
        }
    }
}

/** Small line icons keep the navigation consistent without depending on a glyph font. */
@Composable
internal fun HubIcon(page: HubPage, modifier: Modifier = Modifier, tint: Color = LocalContentColor.current) {
    Canvas(modifier) {
        val scale = size.minDimension / 24f
        val stroke = Stroke(width = 1.7f * scale, cap = StrokeCap.Round, join = StrokeJoin.Round)
        fun p(x: Float, y: Float) = Offset(x * scale, y * scale)
        fun line(x1: Float, y1: Float, x2: Float, y2: Float) {
            drawLine(tint, p(x1, y1), p(x2, y2), strokeWidth = 1.7f * scale, cap = StrokeCap.Round)
        }
        when (page) {
            HubPage.HOME -> {
                val house = Path().apply {
                    moveTo(3f * scale, 10.5f * scale)
                    lineTo(12f * scale, 3.5f * scale)
                    lineTo(21f * scale, 10.5f * scale)
                    moveTo(5.5f * scale, 9.5f * scale)
                    lineTo(5.5f * scale, 20f * scale)
                    lineTo(18.5f * scale, 20f * scale)
                    lineTo(18.5f * scale, 9.5f * scale)
                }
                drawPath(house, tint, style = stroke)
                line(10f, 20f, 10f, 14f)
                line(14f, 20f, 14f, 14f)
                line(10f, 14f, 14f, 14f)
            }
            HubPage.AVATAR -> {
                drawCircle(tint, radius = 3.5f * scale, center = p(12f, 7.5f), style = stroke)
                drawArc(
                    color = tint,
                    startAngle = 190f,
                    sweepAngle = 160f,
                    useCenter = false,
                    topLeft = p(3.5f, 12f),
                    size = Size(17f * scale, 14f * scale),
                    style = stroke,
                )
            }
            HubPage.AI -> {
                val star = Path().apply {
                    moveTo(12f * scale, 3f * scale)
                    lineTo(14f * scale, 9.5f * scale)
                    lineTo(20.5f * scale, 12f * scale)
                    lineTo(14f * scale, 14.5f * scale)
                    lineTo(12f * scale, 21f * scale)
                    lineTo(10f * scale, 14.5f * scale)
                    lineTo(3.5f * scale, 12f * scale)
                    lineTo(10f * scale, 9.5f * scale)
                    close()
                }
                drawPath(star, tint, style = stroke)
            }
            HubPage.VOICE -> {
                line(3.5f, 10f, 3.5f, 14f)
                line(7.5f, 7f, 7.5f, 17f)
                line(12f, 4f, 12f, 20f)
                line(16.5f, 7f, 16.5f, 17f)
                line(20.5f, 10f, 20.5f, 14f)
            }
            HubPage.SETTINGS -> {
                drawCircle(tint, radius = 6.5f * scale, center = p(12f, 12f), style = stroke)
                drawCircle(tint, radius = 2.2f * scale, center = p(12f, 12f), style = stroke)
                repeat(8) { index ->
                    val angle = index * PI / 4.0
                    val dx = cos(angle).toFloat()
                    val dy = sin(angle).toFloat()
                    line(12f + dx * 7f, 12f + dy * 7f, 12f + dx * 9.5f, 12f + dy * 9.5f)
                }
            }
        }
    }
}
