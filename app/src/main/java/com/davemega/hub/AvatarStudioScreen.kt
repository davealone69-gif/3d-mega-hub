package com.davemega.hub

import android.content.ClipData
import android.content.Context
import android.content.Intent
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
import androidx.core.content.FileProvider
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

private const val READY_TO_GENERATE =
    "Ready to generate on this device; the current model stays unchanged until generation succeeds."

@Composable
internal fun AvatarStudioScreen(
    avatarUri: Uri?,
    avatarName: String,
    generationPrompt: String,
    selectedPresetId: String?,
    characterDraft: CharacterDraft,
    onPresetSelected: (String) -> Unit,
    onCharacterDraftChange: (CharacterDraft) -> Unit,
    onGenerationPromptChange: (String) -> Unit,
    onImport: () -> Unit,
    onImportUrl: (String) -> Unit = {},
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
    var diagnosticScreenshot by remember { mutableStateOf<File?>(null) }
    var remoteGlbUrl by remember { mutableStateOf("") }
    var generationStatus by remember { mutableStateOf(READY_TO_GENERATE) }
    var rendererStatus by remember { mutableStateOf("Waiting for AvatarForge GLES or Filament preview surface.") }
    // Imported GLBs open in the animation-capable gltfio preview by default.
    // The native AvatarForge GLES renderer currently does not skin imported GLBs.
    var useFilamentAnimationPreview by remember(avatarUri) {
        // The debug diagnostic must exercise the GLES renderer rather than the default
        // animation-capable route; ordinary generated/imported files still open in Filament.
        mutableStateOf(avatarUri != null && avatarUri.scheme == "file" && avatarUri.path != pixelDiagnosticPath)
    }
    val hasImportedFile = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)?.isFile == true
    // The built-in sample also uses Filament, even though it currently has no animation clips.
    val usingFilamentPreview = !hasImportedFile || useFilamentAnimationPreview
    var filamentPreviewView by remember(avatarUri, usingFilamentPreview) { mutableStateOf<FilamentPreviewView?>(null) }
    var animationPlayback by remember(avatarUri, usingFilamentPreview) { mutableStateOf(AnimationPlaybackState()) }
    var generationJob by remember { mutableStateOf<Job?>(null) }
    // Keep the viewport visible on a normal studio launch, while the home-screen Scout action
    // opens the library directly. Collapsing after a successful build reveals the new model.
    var showCharacterLibrary by remember { mutableStateOf(selectedPresetId != null) }
    val generating = generationJob?.isActive == true
    val selectedPreset = CharacterPresets.find(selectedPresetId)
        ?.takeIf { it.id == characterDraft.presetId && characterDraft.prompt() == generationPrompt.trim() }
    val canGenerate = engineHealth.usable && !generating && !isImporting && generationPrompt.isNotBlank()

    fun generateDraft() {
        if (!canGenerate || generationJob?.isActive == true) return
        // Snapshot the draft before dispatching native work. An edited prompt must not silently
        // inherit the original preset's parameters, even if a stale ID survives a state restore.
        val promptAtStart = generationPrompt.trim()
        val presetAtStart = selectedPreset
        val specAtStart = CharacterPresets.specForDraft(selectedPresetId, promptAtStart, characterDraft)
        generationJob = scope.launch {
            generationStatus = "Generating on this device. Resource and thermal checks run before work begins."
            try {
                pixelDiagnosticPath = null
                diagnosticScreenshot = null
                val result = AvatarForgeEngine.generate(
                    context = context,
                    spec = specAtStart,
                    prompt = promptAtStart,
                )
                val displayName = presetAtStart?.let { "${it.name} · ${result.file.name}" } ?: result.file.name
                onGenerated(Uri.fromFile(result.file), displayName)
                showCharacterLibrary = false
                val warning = result.promptMatchWarning?.let { " · WARNING: $it" }.orEmpty()
                generationStatus = "Generated ${result.triangleCount} triangles · ${result.vertexCount} vertices · ${result.jointCount} joints · ${result.blendshapeCount} blendshapes · ${result.glbBytes / 1024L} KiB GLB · deformed mesh validated=${result.deformedMeshValidated}$warning"
            } catch (cancelled: CancellationException) {
                generationStatus = "Generation cancelled; the current model was not changed."
            } catch (failure: Exception) {
                generationStatus = "Generation failed: ${failure.message ?: failure.javaClass.simpleName}"
            } finally {
                generationJob = null
            }
        }
    }

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("Avatar studio", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        if (isDebuggable) {
            OutlinedButton(
                onClick = {
                    showCharacterLibrary = false
                    val diagnosticPrefs = context.getSharedPreferences("avatarforge_renderer_smoke", Context.MODE_PRIVATE)
                    diagnosticPrefs.edit().clear().putString("status", "GENERATING").commit()
                    generationJob = scope.launch {
                        val diagnosticPrompt = "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans"
                        pixelDiagnosticPath = null
                        diagnosticScreenshot = null
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
                Text(if (generating) "Generation running…" else "Run generated-avatar GLES diagnostic")
            }
        }
        OutlinedButton(
            onClick = { showCharacterLibrary = !showCharacterLibrary },
            enabled = !generating,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(if (showCharacterLibrary) "Hide character presets" else "Browse character presets")
        }
        if (showCharacterLibrary) {
            CharacterPresetSection(
                selected = selectedPreset,
                draft = characterDraft,
                canSelect = !generating && !isImporting,
                canGenerate = canGenerate && selectedPreset != null,
                generating = generating,
                generationStatus = if (engineHealth.usable) generationStatus else
                    "AvatarForge unavailable: ${engineHealth.problem ?: "native engine health check failed"}",
                onSelect = {
                    generationStatus = READY_TO_GENERATE
                    onPresetSelected(it)
                },
                onDraftChange = {
                    generationStatus = READY_TO_GENERATE
                    onCharacterDraftChange(it)
                },
                onGenerate = ::generateDraft,
                onCancel = { generationJob?.cancel() },
            )
        }
        Text("Generated and imported GLBs open in the Filament preview for animation. The AvatarForge GLES preview is currently static.")

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("3D viewport", style = MaterialTheme.typography.titleMedium)
                key(avatarUri, useFilamentAnimationPreview) {
                    val importedGlb = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)
                    if (importedGlb != null && importedGlb.isFile && !useFilamentAnimationPreview) {
                        AndroidView(
                            factory = { viewContext ->
                                AvatarForgeSurfaceView(viewContext, importedGlb, verifyVisiblePixels = isDebuggable) { status ->
                                    android.os.Handler(android.os.Looper.getMainLooper()).post {
                                        rendererStatus = status
                                        val savedPath = context.getSharedPreferences("avatarforge_renderer_smoke", Context.MODE_PRIVATE)
                                            .getString("screenshot_path", null)
                                        diagnosticScreenshot = savedPath?.let(::File)?.takeIf { it.isFile }
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
                                    setAnimationInfoListener { animationPlayback = it }
                                    setModel(avatarUri)
                                    filamentPreviewView = this
                                }
                            },
                            update = { view ->
                                filamentPreviewView = view
                                view.setModel(avatarUri)
                            },
                            modifier = Modifier.fillMaxWidth().height(420.dp)
                        )
                    }
                }
                Text(
                    if (usingFilamentPreview) "Animation uses the clips embedded in this GLB."
                    else "Drag to orbit · Pinch to zoom · Double-tap to reset camera",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                if (hasImportedFile) {
                    OutlinedButton(
                        onClick = { useFilamentAnimationPreview = !useFilamentAnimationPreview },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (useFilamentAnimationPreview) "Use AvatarForge GLES preview" else "Open animation-capable Filament preview")
                    }
                }
                CharacterAnimationControls(
                    usingFilament = usingFilamentPreview,
                    playback = animationPlayback,
                    onSelectClip = { filamentPreviewView?.selectAnimation(it) },
                    onPlayingChange = { filamentPreviewView?.setAnimationPlaying(it) },
                    onStop = { filamentPreviewView?.stopAnimation() },
                    onLoopChange = { filamentPreviewView?.setAnimationLoop(it) },
                    onSpeedChange = { filamentPreviewView?.setAnimationSpeed(it) },
                    onSeek = { filamentPreviewView?.seekAnimation(it) },
                )
                Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
                Text(
                    "Renderer diagnostics are saved automatically to Downloads/3D Mega Hub Diagnostics when a renderer check runs.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                diagnosticScreenshot?.takeIf { it.isFile }?.let { screenshot ->
                    OutlinedButton(
                        onClick = {
                            runCatching {
                                val appFilesRoot = requireNotNull(context.getExternalFilesDir(null)) {
                                    "App-specific external files directory is unavailable."
                                }.canonicalFile
                                val safeFile = screenshot.canonicalFile
                                require(safeFile.isFile && safeFile.parentFile == appFilesRoot) {
                                    "Diagnostic capture is missing or outside the app-private files directory."
                                }
                                val uri = FileProvider.getUriForFile(
                                    context,
                                    "${context.packageName}.fileprovider",
                                    safeFile
                                )
                                val shareIntent = Intent(Intent.ACTION_SEND).apply {
                                    type = "image/png"
                                    putExtra(Intent.EXTRA_STREAM, uri)
                                    clipData = ClipData.newUri(context.contentResolver, safeFile.name, uri)
                                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                                }
                                context.startActivity(
                                    Intent.createChooser(shareIntent, "Save or share renderer capture")
                                )
                            }.onFailure { failure ->
                                rendererStatus = "BLOCKED: could not share renderer capture: ${failure.message ?: failure.javaClass.simpleName}"
                            }
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (screenshot.name == "failed_render.png") "Save / Share failed_render.png" else "Save / Share renderer capture")
                    }
                }
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
                OutlinedTextField(
                    value = remoteGlbUrl,
                    onValueChange = { remoteGlbUrl = it.take(4_096) },
                    label = { Text("HTTPS or Dropbox .glb link") },
                    placeholder = { Text("https://www.dropbox.com/.../model.glb?dl=0") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
                OutlinedButton(
                    onClick = { onImportUrl(remoteGlbUrl) },
                    enabled = !isImporting && !generating && remoteGlbUrl.isNotBlank(),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(if (isImporting) "Downloading GLB…" else "Download & Render URL")
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
                    onValueChange = {
                        generationStatus = READY_TO_GENERATE
                        onGenerationPromptChange(it.take(500))
                    },
                    enabled = !generating && !isImporting,
                    label = { Text("Describe your avatar") },
                    modifier = Modifier.fillMaxWidth(),
                    minLines = 3
                )
                Text("Generation uses AvatarForge's deterministic prompt/measurement parser. It is not photo-to-identity reconstruction or an embedded LLM.", style = MaterialTheme.typography.bodySmall)
                Button(
                    onClick = ::generateDraft,
                    enabled = canGenerate,
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

