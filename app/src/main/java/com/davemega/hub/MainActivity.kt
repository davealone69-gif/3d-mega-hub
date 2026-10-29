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
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView

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
    var rendererStatus by remember { mutableStateOf("Renderer not started. The app shell should remain usable without loading native graphics libraries.") }
    var showRenderer by remember { mutableStateOf(false) }
    val lastRendererCheckpoint = remember {
        context.getSharedPreferences(FilamentPreviewView.DIAGNOSTICS_PREFS, android.content.Context.MODE_PRIVATE)
            .getString(FilamentPreviewView.LAST_CHECKPOINT, "No renderer attempt recorded yet.")
            ?: "No renderer attempt recorded yet."
    }
    val capabilities = listOf(
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
            "Renderer integration test: Filament with an explicitly selected OpenGL ES backend and a bundled GLB model.",
            style = MaterialTheme.typography.bodyMedium
        )

        Text("3D viewport", style = MaterialTheme.typography.titleLarge)
        if (!showRenderer) {
            Text(
                "The renderer is deliberately held back at startup so we can separate an app-shell crash from a native graphics crash.",
                style = MaterialTheme.typography.bodyMedium
            )
            Button(
                onClick = {
                    rendererStatus = "Starting Filament OpenGL ES. The last completed startup checkpoint is shown below."
                    showRenderer = true
                },
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Start 3D renderer test")
            }
        } else {
            Card(modifier = Modifier.fillMaxWidth()) {
                AndroidView(
                    factory = { viewContext ->
                        FilamentPreviewView(viewContext).apply {
                            setStatusListener { rendererStatus = it }
                        }
                    },
                    modifier = Modifier.fillMaxWidth().height(360.dp)
                )
            }
            Button(
                onClick = {
                    showRenderer = false
                    rendererStatus = "Renderer detached. App shell remains active."
                },
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Stop renderer test")
            }
        }
        Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
        Text("Last renderer checkpoint: $lastRendererCheckpoint", style = MaterialTheme.typography.bodySmall)

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
            "This is a renderer integration test, not yet a device-verified release. Avatar generation and AI remain separate integrations.",
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
