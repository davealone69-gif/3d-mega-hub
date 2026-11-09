package com.davemega.hub

import android.content.Context
import android.content.pm.ApplicationInfo
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
import androidx.compose.runtime.key
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
import com.davemega.hub.avatarforge.AvatarForgeSurfaceView
import com.davemega.hub.avatarforge.AvatarSpec
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import java.io.File
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

@Composable
fun AvatarStudioScreen(
    avatarUri: Uri?,
    avatarName: String,
    onImport: () -> Unit,
    onRemove: () -> Unit,
    onGenerated: (Uri, String) -> Unit,
    isImporting: Boolean = false,
    importStatus: String = ""
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val engineHealth = remember { AvatarForgeEngine.health() }
    val isDebuggable = remember { (context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0 }
    var pixelDiagnosticPath by remember { mutableStateOf<String?>(null) }
    var generationPrompt by remember {
        mutableStateOf("Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans")
    }
    var generationStatus by remember { mutableStateOf("On-device generation has not been run in this session.") }
    var rendererStatus by remember { mutableStateOf("Waiting for AvatarForge GLES or Filament preview surface.") }
    var generationJob by remember { mutableStateOf<Job?>(null) }
    val generating = generationJob?.isActive == true

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("Avatar studio", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        if (isDebuggable) {
            OutlinedButton(
                onClick = {
                    val diagnosticPrefs = context.getSharedPreferences("avatarforge_renderer_smoke", Context.MODE_PRIVATE)
                    diagnosticPrefs.edit().clear().putString("status", "GENERATING").commit()
                    generationJob = scope.launch {
                        val diagnosticPrompt = "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans"
                        pixelDiagnosticPath = null
                        generationStatus = "Generating deterministic seed-1337 avatar; validating deformed vertices and indexed submesh bounds before export."
                        try {
                            val result = AvatarForgeEngine.generate(
                                context = context,
                                spec = AvatarSpec(
                                    sex = Sex.FEMALE,
                                    build = Build.ATHLETIC,
                                    hairStyle = HairStyle.MEDIUM,
                                    outfit = Outfit.JEANS_JACKET,
                                    seed = 1337,
                                    heightM = 1.75f,
                                    prompt = diagnosticPrompt
                                ),
                                prompt = diagnosticPrompt,
                                outputName = "diagnostic-seed-1337-${System.currentTimeMillis()}.glb"
                            )
                            check(result.deformedMeshValidated) { "Native deformed-mesh validation was not confirmed." }
                            pixelDiagnosticPath = result.file.absolutePath
                            diagnosticPrefs.edit()
                                .putString("generation_report", "seed=1337 vertices=${result.vertexCount} triangles=${result.triangleCount} joints=${result.jointCount} blendshapes=${result.blendshapeCount} bytes=${result.glbBytes} deformed_mesh_validated=${result.deformedMeshValidated}")
                                .commit()
                            onGenerated(Uri.fromFile(result.file), result.file.name)
                            generationStatus = "Native generation and deformed-mesh validation passed. Waiting for successful GLES swaps and PixelCopy visibility proof."
                        } catch (cancelled: CancellationException) {
                            diagnosticPrefs.edit().putString("status", "FAILED").putString("error", "Diagnostic cancelled").commit()
                            generationStatus = "Generated-avatar GLES diagnostic cancelled."
                        } catch (failure: Exception) {
                            val message = failure.message ?: failure.javaClass.simpleName
                            diagnosticPrefs.edit().putString("status", "FAILED").putString("error", "Generation failed: $message").commit()
                            generationStatus = "Generated-avatar GLES diagnostic failed: $message"
                        } finally {
                            generationJob = null
                        }
                    }
                },
                enabled = engineHealth.usable && !generating && !isImporting,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(if (generating) "Diagnostic running…" else "Run generated-avatar GLES diagnostic")
            }
        }
        Text("Imported GLBs use the AvatarForge engine and its native GLES renderer. The bundled sample remains on the existing viewer until it is imported into the engine.")

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("3D viewport", style = MaterialTheme.typography.titleMedium)
                key(avatarUri) {
                    val importedGlb = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)
                    if (importedGlb != null && importedGlb.isFile) {
                        AndroidView(
                            factory = { viewContext ->
                                AvatarForgeSurfaceView(viewContext, importedGlb, verifyVisiblePixels = pixelDiagnosticPath == importedGlb.absolutePath) { status ->
                                    android.os.Handler(android.os.Looper.getMainLooper()).post {
                                        rendererStatus = status
                                    }
                                }
                            },
                            modifier = Modifier.fillMaxWidth().height(420.dp)
                        )
                    } else {
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
                    }
                }
                Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
                Text("Selected asset: $avatarName", style = MaterialTheme.typography.bodySmall)
                if (importStatus.isNotBlank()) {
                    Text(importStatus, style = MaterialTheme.typography.bodySmall)
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = onImport, enabled = !isImporting && !generating) {
                        Text(if (isImporting) "Importing…" else "Import GLB")
                    }
                    if (avatarUri != null) OutlinedButton(onClick = onRemove, enabled = !isImporting && !generating) { Text("Use sample model") }
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
                    onValueChange = { generationPrompt = it.take(500) },
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
                                pixelDiagnosticPath = null
                                val result = AvatarForgeEngine.generate(
                                    context = context,
                                    spec = AvatarSpec(prompt = generationPrompt.trim()),
                                    prompt = generationPrompt.trim()
                                )
                                onGenerated(Uri.fromFile(result.file), result.file.name)
                                val warning = result.promptMatchWarning?.let { " · WARNING: $it" }.orEmpty()
                                generationStatus = "Generated ${result.triangleCount} triangles · ${result.vertexCount} vertices · ${result.jointCount} joints · ${result.blendshapeCount} blendshapes · ${result.glbBytes / 1024L} KiB GLB · deformed mesh validated=${result.deformedMeshValidated}$warning"
                            } catch (cancelled: CancellationException) {
                                generationStatus = "Generation cancelled."
                            } catch (failure: Exception) {
                                generationStatus = "Generation failed: ${failure.message ?: failure.javaClass.simpleName}"
                            } finally {
                                generationJob = null
                            }
                        }
                    },
                    enabled = engineHealth.usable && !generating && !isImporting && generationPrompt.isNotBlank(),
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
