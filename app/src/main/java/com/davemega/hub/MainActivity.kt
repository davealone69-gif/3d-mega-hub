package com.davemega.hub

import android.content.Context
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
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) { MegaHubHome() }
            }
        }
    }
}

@Composable
private fun MegaHubHome() {
    val context = LocalContext.current
    val clipboard = LocalClipboardManager.current
    val diagnostics = remember {
        context.getSharedPreferences(FilamentPreviewView.DIAGNOSTICS_PREFS, Context.MODE_PRIVATE)
    }
    var rendererStatus by remember {
        mutableStateOf("Renderer not started. Tap Start 3D renderer test to attempt native initialisation.")
    }
    var lastRendererCheckpoint by remember {
        mutableStateOf(diagnostics.getString(FilamentPreviewView.LAST_CHECKPOINT, "No renderer attempt recorded yet.") ?: "No renderer attempt recorded yet.")
    }
    var showRenderer by remember { mutableStateOf(false) }

    val diagnosticReport = """
3D Mega Hub diagnostic report
Device: ${Build.MODEL.ifBlank { "Unknown" }}
Manufacturer: ${Build.MANUFACTURER.ifBlank { "Unknown" }}
Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})
Supported ABIs: ${Build.SUPPORTED_ABIS.joinToString().ifBlank { "Unknown" }}
Native library directory: ${context.applicationInfo.nativeLibraryDir}
Package: ${context.packageName}

Live renderer status:
$rendererStatus

Last renderer checkpoint:
$lastRendererCheckpoint

Test scope:
Filament OpenGL ES initialisation and frame submission only.
A GLB avatar is not loaded in this diagnostic build.
A submitted frame does not prove that an avatar is visible.
""".trimIndent()

    val capabilities = listOf(
        RuntimeCapability(name = "Avatar generation engine"),
        RuntimeCapability(name = "Embedded AI / LLM"),
        RuntimeCapability(name = "Voice and lip-sync"),
        RuntimeCapability(name = "GLB avatar loading")
    )

    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text("3D Mega Hub", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold)
        Text(
            "Native renderer diagnostic: Filament OpenGL ES initialisation and frame submission. A GLB avatar is not loaded in this diagnostic build.",
            style = MaterialTheme.typography.bodyMedium
        )

        Text("3D viewport", style = MaterialTheme.typography.titleLarge)
        if (!showRenderer) {
            Text(
                "The renderer starts only when requested, so a native graphics failure cannot prevent the app shell opening.",
                style = MaterialTheme.typography.bodyMedium
            )
            Button(
                onClick = {
                    rendererStatus = "Creating renderer view and waiting for Android surface..."
                    showRenderer = true
                },
                modifier = Modifier.fillMaxWidth()
            ) { Text("Start 3D renderer test") }
        } else {
            Card(modifier = Modifier.fillMaxWidth()) {
                AndroidView(
                    factory = { viewContext ->
                        FilamentPreviewView(viewContext).apply {
                            setStatusListener { status ->
                                rendererStatus = status
                                lastRendererCheckpoint = diagnostics.getString(FilamentPreviewView.LAST_CHECKPOINT, status) ?: status
                            }
                        }
                    },
                    modifier = Modifier.fillMaxWidth().height(360.dp)
                )
            }
            Text("Renderer remains mounted during the test. Leave this screen to end it.", style = MaterialTheme.typography.bodySmall)
        }

        Text("Live renderer status", style = MaterialTheme.typography.titleMedium)
        Text(rendererStatus, style = MaterialTheme.typography.bodyMedium)
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
                DetailRow("Native library directory", context.applicationInfo.nativeLibraryDir)
                DetailRow("Package", context.packageName)
            }
        }
        Text(
            "NOT INTEGRATED means there is no connected backend. Renderer initialisation and visible avatar rendering are separate verification gates.",
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
                Text("No implementation is connected in this build.", style = MaterialTheme.typography.bodySmall)
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
