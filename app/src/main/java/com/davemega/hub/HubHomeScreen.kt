package com.davemega.hub

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
internal fun HubHomeScreen(
    avatarName: String,
    connection: String,
    voiceStatus: String,
    onCreateScout: () -> Unit,
    onOpenAvatar: () -> Unit,
    onOpenAi: () -> Unit,
    onOpenVoice: () -> Unit,
    onOpenSettings: () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
        Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Text("YOUR CREATIVE WORKSPACE", color = HubColors.Accent, style = MaterialTheme.typography.labelSmall, letterSpacing = 1.5.sp)
            Text("Bring characters to life.", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold)
            Text("Create, explore and make it your own.", color = HubColors.Muted, style = MaterialTheme.typography.bodyMedium)
        }

        ScoutFeature(onCreateScout)

        Surface(
            shape = RoundedCornerShape(16.dp),
            color = HubColors.Surface,
            border = androidx.compose.foundation.BorderStroke(1.dp, HubColors.Outline.copy(alpha = 0.65f)),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(start = 16.dp, end = 8.dp, top = 10.dp, bottom = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(3.dp)) {
                    Text("CURRENT MODEL", color = HubColors.AccentMuted, style = MaterialTheme.typography.labelSmall, letterSpacing = 1.sp)
                    Text(avatarName, maxLines = 1, overflow = TextOverflow.Ellipsis, style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                }
                TextButton(onClick = onOpenAvatar) { Text("Open studio  →") }
            }
        }

        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.SpaceBetween) {
            Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
                Text("YOUR TOOLS", color = HubColors.Accent, style = MaterialTheme.typography.labelSmall, letterSpacing = 1.5.sp)
                Text("The workspace", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
            }
            Text("EXPLORE  /  03", color = HubColors.Muted, style = MaterialTheme.typography.labelSmall, letterSpacing = 0.8.sp)
        }

        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            WorkspaceTile(
                page = HubPage.AI,
                title = "Local AI",
                detail = connection,
                onClick = onOpenAi,
                modifier = Modifier.weight(1f),
            )
            WorkspaceTile(
                page = HubPage.VOICE,
                title = "Voice",
                detail = voiceStatus,
                onClick = onOpenVoice,
                modifier = Modifier.weight(1f),
            )
        }
        Surface(
            shape = RoundedCornerShape(16.dp),
            color = HubColors.Surface,
            border = androidx.compose.foundation.BorderStroke(1.dp, HubColors.Outline.copy(alpha = 0.65f)),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().clickable(role = Role.Button, onClick = onOpenSettings).padding(16.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                HubIcon(HubPage.SETTINGS, Modifier.size(22.dp), tint = HubColors.Secondary)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text("Device & settings", fontWeight = FontWeight.SemiBold)
                    Text("Graphics, endpoint and system info", color = HubColors.Muted, style = MaterialTheme.typography.bodySmall)
                }
                Text("↗", color = HubColors.Accent, style = MaterialTheme.typography.titleMedium)
            }
        }
        Spacer(Modifier.height(4.dp))
    }
}

@Composable
private fun ScoutFeature(onCreateScout: () -> Unit) {
    val shape = RoundedCornerShape(20.dp)
    Box(
        Modifier.fillMaxWidth().height(400.dp).clip(shape)
            .background(HubColors.SurfaceRaised)
            .border(1.dp, Color.White.copy(alpha = 0.12f), shape),
    ) {
        Image(
            painter = painterResource(R.drawable.scout_concept),
            contentDescription = null, // Decorative concept art; the label below explains what it is.
            modifier = Modifier.fillMaxSize(),
            contentScale = ContentScale.Crop,
            alignment = Alignment.Center,
        )
        Box(
            Modifier.fillMaxSize().background(
                Brush.horizontalGradient(
                    0f to Color(0xDE12151C),
                    0.55f to Color(0x5312161D),
                    1f to Color.Transparent,
                )
            )
        )
        Box(
            Modifier.fillMaxSize().background(
                Brush.verticalGradient(
                    0f to Color.Transparent,
                    0.42f to Color(0x16101318),
                    1f to Color(0xF0101318),
                )
            )
        )
        Text(
            "CHARACTER  /  01",
            modifier = Modifier.align(Alignment.TopStart).padding(18.dp),
            color = HubColors.Accent,
            style = MaterialTheme.typography.labelSmall,
            fontWeight = FontWeight.SemiBold,
            letterSpacing = 1.5.sp,
        )
        Column(
            modifier = Modifier.align(Alignment.BottomStart).fillMaxWidth(0.72f).padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(7.dp),
        ) {
            Text("THE SCOUT", color = Color.White, fontWeight = FontWeight.Black, fontSize = 29.sp, lineHeight = 31.sp, letterSpacing = 1.sp)
            Text("A survivor, ready for your story.", color = Color(0xFFE3E4D9), style = MaterialTheme.typography.bodySmall)
            Button(
                onClick = onCreateScout,
                colors = ButtonDefaults.buttonColors(containerColor = HubColors.Accent, contentColor = Color(0xFF371B29)),
            ) { Text("Create Scout  →", fontWeight = FontWeight.Bold) }
            Text("CONCEPT ART · 3D PRESET", color = Color(0xFFDDD9CB), style = MaterialTheme.typography.labelSmall, letterSpacing = 0.7.sp)
        }
    }
}

@Composable
private fun WorkspaceTile(page: HubPage, title: String, detail: String, onClick: () -> Unit, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(16.dp),
        color = HubColors.Surface,
        border = androidx.compose.foundation.BorderStroke(1.dp, HubColors.Outline.copy(alpha = 0.65f)),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().heightIn(min = 142.dp)
                .clickable(role = Role.Button, onClick = onClick).padding(15.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Box(
                    Modifier.size(38.dp).background(HubColors.SurfaceRaised, RoundedCornerShape(11.dp)),
                    contentAlignment = Alignment.Center,
                ) { HubIcon(page, Modifier.size(20.dp), tint = HubColors.Secondary) }
                Text("↗", color = HubColors.Accent, style = MaterialTheme.typography.titleMedium)
            }
            Text(title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
            Text(detail, color = HubColors.Muted, style = MaterialTheme.typography.bodySmall, maxLines = 2, overflow = TextOverflow.Ellipsis)
        }
    }
}
