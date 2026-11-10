package com.davemega.hub

import android.content.ClipData
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Environment
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
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import java.io.File

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
    val capabilities = remember {
        listOf(
            RuntimeCapability(name = "3D renderer"),
            RuntimeCapability(name = "Avatar generation engine"),
            RuntimeCapability(name = "Embedded AI"),
            RuntimeCapability(name = "Voice and lip-sync")
        )
    }
    val diagnosticSnapshot = remember { mutableStateOf(RendererDiagnosticSnapshot()) }
    val shareStatus = remember { mutableStateOf("No diagnostic image has been exported yet.") }

    val diagnosticFile = File(
        context.getExternalFilesDir(Environment.DIRECTORY_PICTURES),
        "diagnostics/failed_render.png"
    )

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text(
            "3D Mega Hub",
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold
        )
        Text(
            "Android foundation build. Renderer, AI and generation backends are not connected yet.",
            style = MaterialTheme.typography.bodyMedium
        )

        Text("Capability status", style = MaterialTheme.typography.titleLarge)
        capabilities.forEach { capability -> CapabilityCard(capability) }

        Text("Renderer diagnostics", style = MaterialTheme.typography.titleLarge)
        RendererDiagnosticOverlay(snapshot = diagnosticSnapshot.value)

        Button(
            onClick = {
                if (!diagnosticFile.isFile) {
                    shareStatus.value = "MISSING: failed_render.png has not been generated."
                } else {
                    runCatching {
                        val uri = FileProvider.getUriForFile(
                            context,
                            "${context.packageName}.fileprovider",
                            diagnosticFile
                        )
                        val sendIntent = Intent(Intent.ACTION_SEND).apply {
                            type = "image/png"
                            putExtra(Intent.EXTRA_STREAM, uri)
                            clipData = ClipData.newUri(
                                context.contentResolver,
                                "Failed render diagnostic",
                                uri
                            )
                            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        }
                        context.startActivity(
                            Intent.createChooser(sendIntent, "Save or share failed_render.png")
                        )
                        shareStatus.value = "Share sheet opened for failed_render.png."
                    }.onFailure { error ->
                        shareStatus.value =
                            "BLOCKED: could not share diagnostic image: ${error.message ?: error.javaClass.simpleName}"
                    }
                }
            },
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Save / Share failed_render.png")
        }
        Text(
            "Expected file: Android/data/${context.packageName}/files/Pictures/diagnostics/failed_render.png",
            style = MaterialTheme.typography.bodySmall
        )
        Text(shareStatus.value, style = MaterialTheme.typography.bodySmall)

        Text("Device snapshot", style = MaterialTheme.typography.titleLarge)
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                DetailRow("Device", Build.MODEL.ifBlank { "Unknown" })
                DetailRow("Manufacturer", Build.MANUFACTURER.ifBlank { "Unknown" })
                DetailRow("Android", "${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
                DetailRow("Supported ABIs", Build.SUPPORTED_ABIS.joinToString().ifBlank { "Unknown" })
                DetailRow("Package", context.packageName)
            }
        }
        Text(
            "The diagnostic overlay reports NOT RUN until a real renderer publishes measurements. " +
                "This foundation build does not claim visible 3D output.",
            style = MaterialTheme.typography.bodySmall
        )
    }
}

@Composable
private fun CapabilityCard(capability: RuntimeCapability) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp)
        ) {
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
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text("$label:", fontWeight = FontWeight.SemiBold)
        Text(value)
    }
}
