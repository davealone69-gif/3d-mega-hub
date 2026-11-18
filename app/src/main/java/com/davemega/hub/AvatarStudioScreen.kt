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
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.key
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
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
import com.davemega.hub.avatarforge.GlbValidator
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import java.io.File
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun AvatarStudioScreen(
    avatarUri: Uri?,
    avatarName: String,
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
    var generationPrompt by remember {
        mutableStateOf("Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans")
    }
    var generationStatus by remember { mutableStateOf("On-device generation has not been run in this session.") }
    var rendererStatus by remember { mutableStateOf("Waiting for AvatarForge GLES or Filament preview surface.") }
    // The gltfio preview is NOT the default: it aborts the process on load in every Filament
    // version this project has tested (1.71.6, 1.79.0, 1.80.1), see
    // docs/BLOCKER_FILAMENT_PREVIEW_GLTFIO_PANIC.md. Opening a screen must never kill the app, so
    // the GLES renderer stays the default until that is resolved.
    var useFilamentAnimationPreview by remember(avatarUri) { mutableStateOf(false) }
    var filamentPreviewView by remember { mutableStateOf<FilamentPreviewView?>(null) }
    var animationCount by remember { mutableStateOf(0) }
    var selectedAnimation by remember { mutableStateOf(0) }
    var animationPlaying by remember { mutableStateOf(false) }
    var animationLoop by remember { mutableStateOf(true) }
    var animationSpeed by remember { mutableStateOf(1.0f) }
    var animationDuration by remember { mutableStateOf(0.0f) }
    var animationPosition by remember { mutableStateOf(0.0f) }
    val hasImportedFile = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)?.isFile == true
    var generationJob by remember { mutableStateOf<Job?>(null) }
    val generating = generationJob?.isActive == true
    // Structural clip inventory read from the GLB's own JSON chunk. Filament can only publish
    // clips the file declares, so this is what separates "the asset has no clips" from "the
    // viewer has not published them" when no animation controls appear.
    var clipInventory by remember(avatarUri) { mutableStateOf<GlbValidator.AnimationInventory?>(null) }
    var clipInventoryError by remember(avatarUri) { mutableStateOf<String?>(null) }
    val appearancePrefs = remember { context.getSharedPreferences("mega_hub", Context.MODE_PRIVATE) }
    var appearance by remember {
        mutableStateOf(AppearanceStore.decode { key -> appearancePrefs.getString(key, null) })
    }
    fun applyAppearance(next: AvatarAppearance) {
        appearance = next
        val editor = appearancePrefs.edit()
        AppearanceStore.encode(next).forEach { (key, value) -> editor.putString(key, value) }
        editor.apply()
    }
    LaunchedEffect(avatarUri) {
        clipInventory = null
        clipInventoryError = null
        val selected = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)?.takeIf { it.isFile }
            ?: return@LaunchedEffect
        withContext(Dispatchers.IO) { runCatching { GlbValidator.inspectAnimations(selected) } }
            .onSuccess { clipInventory = it }
            .onFailure { clipInventoryError = it.message ?: it.javaClass.simpleName }
    }

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
                Text(if (generating) "Diagnostic running…" else "Run generated-avatar GLES diagnostic")
            }
        }
        Text("Imported GLBs open in the Filament gltfio preview so embedded skeletal animation can play. Switch to AvatarForge GLES preview for the native static-render path.")

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("3D viewport", style = MaterialTheme.typography.titleMedium)
                key(avatarUri, useFilamentAnimationPreview) {
                    val importedGlb = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)
                    if (importedGlb != null && importedGlb.isFile) {
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
                        Text(
                            "No model is loaded. Import a GLB to preview it through the AvatarForge " +
                                "GLES renderer. The animation-capable Filament preview is disabled in " +
                                "this build because gltfio aborts on load; see " +
                                "docs/BLOCKER_FILAMENT_PREVIEW_GLTFIO_PANIC.md.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
                Text(
                    "Drag to orbit · Pinch to zoom · Double-tap to reset camera",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
                if (hasImportedFile && !useFilamentAnimationPreview) {
                    Text(
                        "AvatarForge GLES preview: the imported model is drawn through the native " +
                            "static path, which does not apply imported skeletal animation. Expect the bind " +
                            "pose and no animation controls here — switch to the Filament preview below for " +
                            "clip playback.",
                        style = MaterialTheme.typography.bodySmall
                    )
                }
                if (hasImportedFile) {
                    // Disabled rather than removed: the control is real work that becomes usable the
                    // moment the loader is fixed, but offering a button that aborts the process would
                    // be worse than offering none.
                    OutlinedButton(
                        onClick = { },
                        enabled = false,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Animation preview (Filament) unavailable: gltfio aborts on load")
                    }
                }
                if (useFilamentAnimationPreview) {
                    if (animationCount > 0) {
                        Text("GLB animations · ${selectedAnimation + 1} of $animationCount · ${"%.2f".format(animationDuration)} s")
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            OutlinedButton(
                                onClick = { filamentPreviewView?.selectAnimation((selectedAnimation - 1).coerceAtLeast(0)) },
                                enabled = selectedAnimation > 0
                            ) { Text("Previous") }
                            Button(onClick = { filamentPreviewView?.setAnimationPlaying(!animationPlaying) }) {
                                Text(if (animationPlaying) "Pause" else "Play")
                            }
                            OutlinedButton(onClick = { filamentPreviewView?.stopAnimation() }) {
                                Text("Stop / reset")
                            }
                            OutlinedButton(
                                onClick = { filamentPreviewView?.selectAnimation((selectedAnimation + 1).coerceAtMost(animationCount - 1)) },
                                enabled = selectedAnimation < animationCount - 1
                            ) { Text("Next") }
                        }
                        if (animationDuration > 0f) {
                            Slider(
                                value = animationPosition.coerceIn(0f, animationDuration),
                                onValueChange = { value ->
                                    animationPosition = value
                                    filamentPreviewView?.seekAnimation(value)
                                },
                                valueRange = 0f..animationDuration
                            )
                            Text("${"%.2f".format(animationPosition)} / ${"%.2f".format(animationDuration)} seconds")
                        }
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            OutlinedButton(onClick = {
                                animationLoop = !animationLoop
                                filamentPreviewView?.setAnimationLoop(animationLoop)
                            }) { Text(if (animationLoop) "Loop: on" else "Loop: off") }
                            listOf(0.5f, 1.0f, 1.5f, 2.0f).forEach { speed ->
                                OutlinedButton(onClick = {
                                    animationSpeed = speed
                                    filamentPreviewView?.setAnimationSpeed(speed)
                                }) { Text("${speed}x") }
                            }
                        }
                    } else {
                        val declared = clipInventory
                        val clipLabel = declared?.animationNames?.take(6)?.joinToString(", ").orEmpty()
                        val moreClips = (declared?.animationCount ?: 0) > 6
                        Text(
                            when {
                                clipInventoryError != null ->
                                    "Could not read this GLB's clip table ($clipInventoryError), so the Filament " +
                                        "preview cannot be told apart from a file that simply has no clips."
                                declared == null -> "Reading this GLB's clip table…"
                                declared.declaresClips ->
                                    "This GLB declares ${declared.animationCount} clip(s) and ${declared.skinCount} " +
                                        "skin(s) — $clipLabel${if (moreClips) ", …" else ""} — but Filament has not " +
                                        "published them yet. If the status line above reports a load failure, that is why " +
                                        "no controls appeared."
                                else ->
                                    "This GLB declares no animation clips and ${declared.skinCount} skin(s), so there " +
                                        "is nothing to play and this file will never produce animation controls. Import a " +
                                        "GLB that contains an animations array."
                            },
                            style = MaterialTheme.typography.bodySmall
                        )
                    }
                }
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
                Text("Appearance", style = MaterialTheme.typography.titleMedium)
                Text(
                    "Applied to Generate avatar. The prompt still drives measurements and detail; these controls " +
                        "set the attributes the prompt parser would otherwise have to guess.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                AppearanceCycler(
                    label = "Sex",
                    value = appearance.sex.label(),
                    onPrevious = { applyAppearance(appearance.copy(sex = Sex.entries.cycle(appearance.sex, false))) },
                    onNext = { applyAppearance(appearance.copy(sex = Sex.entries.cycle(appearance.sex, true))) }
                )
                AppearanceCycler(
                    label = "Build",
                    value = appearance.build.label(),
                    onPrevious = { applyAppearance(appearance.copy(build = Build.entries.cycle(appearance.build, false))) },
                    onNext = { applyAppearance(appearance.copy(build = Build.entries.cycle(appearance.build, true))) }
                )
                AppearanceCycler(
                    label = "Hair style",
                    value = appearance.hairStyle.label(),
                    onPrevious = {
                        applyAppearance(appearance.copy(hairStyle = HairStyle.entries.cycle(appearance.hairStyle, false)))
                    },
                    onNext = {
                        applyAppearance(appearance.copy(hairStyle = HairStyle.entries.cycle(appearance.hairStyle, true)))
                    }
                )
                AppearanceCycler(
                    label = "Hair colour",
                    value = appearance.hairColour.label,
                    onPrevious = {
                        applyAppearance(appearance.copy(hairColour = HairColour.entries.cycle(appearance.hairColour, false)))
                    },
                    onNext = {
                        applyAppearance(appearance.copy(hairColour = HairColour.entries.cycle(appearance.hairColour, true)))
                    }
                )
                AppearanceCycler(
                    label = "Outfit",
                    value = appearance.outfit.label(),
                    onPrevious = {
                        applyAppearance(appearance.copy(outfit = AvatarAppearance.selectableOutfits.cycle(appearance.outfit, false)))
                    },
                    onNext = {
                        applyAppearance(appearance.copy(outfit = AvatarAppearance.selectableOutfits.cycle(appearance.outfit, true)))
                    }
                )
                AppearanceCycler(
                    label = "Cloth colour",
                    value = appearance.clothColour.label,
                    onPrevious = {
                        applyAppearance(appearance.copy(clothColour = ClothColour.entries.cycle(appearance.clothColour, false)))
                    },
                    onNext = {
                        applyAppearance(appearance.copy(clothColour = ClothColour.entries.cycle(appearance.clothColour, true)))
                    }
                )
                Text("Skin tone · melanin ${"%.2f".format(appearance.normalized().skinMelanin)}")
                Slider(
                    value = appearance.normalized().skinMelanin,
                    onValueChange = { applyAppearance(appearance.copy(skinMelanin = it)) },
                    valueRange = 0f..1f
                )
                Text("Height · ${"%.0f".format(appearance.normalized().heightCm)} cm")
                Slider(
                    value = appearance.normalized().heightCm,
                    onValueChange = { applyAppearance(appearance.copy(heightCm = it)) },
                    valueRange = 140f..220f
                )
                Text(
                    "Will generate: ${appearance.describe()}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
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
                                    spec = appearance.toSpec(prompt = generationPrompt.trim()),
                                    prompt = generationPrompt.trim()
                                )
                                onGenerated(Uri.fromFile(result.file), result.file.name)
                                val warning = result.promptMatchWarning?.let { " · WARNING: $it" }.orEmpty()
                                generationStatus = "${appearance.describe()} · Generated ${result.triangleCount} triangles · ${result.vertexCount} vertices · ${result.jointCount} joints · ${result.blendshapeCount} blendshapes · ${result.glbBytes / 1024L} KiB GLB · deformed mesh validated=${result.deformedMeshValidated}$warning"
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

/** Human-facing label for an engine enum constant. */
private fun Enum<*>.label(): String = name.lowercase().replace('_', ' ')

/** Wraps around the list so the cyclers can never land outside the valid values. */
private fun <T> List<T>.cycle(current: T, forward: Boolean): T {
    if (isEmpty()) return current
    val index = indexOf(current).takeIf { it >= 0 } ?: 0
    val step = if (forward) 1 else -1
    return this[(index + step + size) % size]
}

/** Previous / value / next row used by the appearance controls. */
@Composable
private fun AppearanceCycler(
    label: String,
    value: String,
    onPrevious: () -> Unit,
    onNext: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text(label, style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(1f))
        OutlinedButton(onClick = onPrevious) { Text("←") }
        Text(value, style = MaterialTheme.typography.bodySmall, modifier = Modifier.weight(1.2f))
        OutlinedButton(onClick = onNext) { Text("→") }
    }
}
