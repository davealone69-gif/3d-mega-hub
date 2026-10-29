package com.davemega.hub

import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    MegaHubHome()
                }
            }
        }
    }
}

@Composable
private fun MegaHubHome() {
    val context = LocalContext.current
    val capabilities = listOf(
        RuntimeCapability(name = "3D renderer"),
        RuntimeCapability(name = "Avatar generation engine"),
        RuntimeCapability(name = "Embedded AI"),
        RuntimeCapability(name = "Voice and lip-sync")
    )

    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text("3D Mega Hub", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold)
        Text(
            "Android foundation build. This screen reports integration state honestly; no renderer, AI or generation service is claimed to be connected yet.",
            style = MaterialTheme.typography.bodyMedium
        )
        Text("Capability status", style = MaterialTheme.typography.titleLarge)
        capabilities.forEach { capability -> CapabilityCard(capability) }

        Text("Device snapshot", style = MaterialTheme.typography.titleLarge)
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                DetailRow("Device", Build.MODEL.ifBlank { "Unknown" })
                DetailRow("Manufacturer", Build.MANUFACTURER.ifBlank { "Unknown" })
                DetailRow("Android", "${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
                DetailRow("Supported ABIs", Build.SUPPORTED_ABIS.joinToString().ifBlank { "Unknown" })
                DetailRow("Package", context.packageName)
            }
        }
        Text(
            "Next gate: establish a green build, then integrate one renderer and verify visible model pixels before adding generation or AI.",
            style = MaterialTheme.typography.bodySmall
        )
    }
}

@Composable
private fun CapabilityCard(capability: RuntimeCapability) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(capability.name, style = MaterialTheme.typography.titleMedium)
            Text(capability.status, style = MaterialTheme.typography.labelLarge)
            if (capability.backend.isNullOrBlank()) {
                Text("No backend is connected in this build.", style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}

@Composable
private fun DetailRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("$label:", fontWeight = FontWeight.SemiBold)
        Text(value)
    }
}
