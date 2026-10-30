package com.davemega.hub

import android.net.Uri
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.davemega.hub.avatarforge.AvatarForgeEngine
import com.davemega.hub.avatarforge.AvatarSpec
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

@Composable
fun AvatarStudioScreen(
    avatarUri: Uri?,
    avatarName: String,
    onImport: () -> Unit,
    onRemove: () -> Unit,
    onGenerated: (Uri, String) -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val engineHealth = remember { AvatarForgeEngine.health() }
    var generationPrompt by remember {
        mutableStateOf("Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans")
    }
    var generationStatus by remember { mutableStateOf("On-device generation has not been run in this session.") }
    var rendererStatus by remember { mutableStateOf("Waiting for Filament surface.") }
    var generationJob by remember { mutableStateOf<Job?>(null) }
    val generating = generationJob?.isActive == true

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("Avatar studio", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        Text("A real Filament glTF viewer with the AvatarForge generation kernel. The bundled sample loads automatically; import a self-contained GLB or generate a new avatar below.")

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("3D viewport", style = MaterialTheme.typography.titleMedium)
                AndroidView(
                    factory = { viewContext ->
                        FilamentPreviewView(viewContext).apply {
                            setStatusListener { rendererStatus = it }
                            setModel(avatarUri)
                        }
                    },
                    update = { view -> view.setModel(avatarUri) },
                    modifier = Modifier.fillMaxWidth().height(420.dp)
                )
                Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
                Text("Selected asset: $avatarName", style = MaterialTheme.typography.bodySmall)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = onImport) { Text("Import GLB") }
                    if (avatarUri != null) OutlinedButton(onClick = onRemove) { Text("Use sample model") }
                }
            }
        }

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("AvatarForge engine", style = MaterialTheme.typography.titleMedium)
                if (engineHealth.usable) {
                    Text("Native library loaded · ABI ${engineHealth.abiVersion} · ${engineHealth.engineVersion}")
                    Text("The native engine is loaded; generation and rendering still require a real phone run.", style = MaterialTheme.typography.bodySmall)
                } else {
                    Text("UNAVAILABLE: ${engineHealth.problem ?: "native engine health check failed"}")
                }
                OutlinedTextField(
                    value = generationPrompt,
                    onValueChange = { generationPrompt = it },
                    label = { Text("Describe your avatar") },
                    modifier = Modifier.fillMaxWidth(),
                    minLines = 3
                )
                Text("Generation uses AvatarForge's deterministic prompt/measurement parser. It is not photo-to-identity reconstruction or an embedded LLM.", style = MaterialTheme.typography.bodySmall)
                Button(
                    onClick = {
                        generationJob = scope.launch {
                            generationStatus = "Running native generation on this device. Resource and thermal gates are checked before work begins."
                            try {
                                val result = AvatarForgeEngine.generate(
                                    context = context,
                                    spec = AvatarSpec(prompt = generationPrompt.trim()),
                                    prompt = generationPrompt.trim()
                                )
                                onGenerated(Uri.fromFile(result.file), result.file.name)
                                generationStatus = "Generated ${result.triangleCount} triangles · ${result.vertexCount} vertices · ${result.jointCount} joints · ${result.blendshapeCount} blendshapes · ${result.glbBytes / 1024L} KiB GLB"
                            } catch (cancelled: CancellationException) {
                                generationStatus = "Generation cancelled."
                            } catch (failure: Exception) {
                                generationStatus = "Generation failed: ${failure.message ?: failure.javaClass.simpleName}"
                            } finally {
                                generationJob = null
                            }
                        }
                    },
                    enabled = engineHealth.usable && !generating && generationPrompt.isNotBlank(),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(if (generating) "Generating…" else "Generate avatar")
                }
                if (generating) {
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        CircularProgressIndicator()
                        OutlinedButton(onClick = { generationJob?.cancel() }) { Text("Cancel generation") }
                    }
                }
                Text(generationStatus, style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}
