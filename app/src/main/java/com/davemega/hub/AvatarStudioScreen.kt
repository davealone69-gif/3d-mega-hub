package com.davemega.hub

import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.net.Uri
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
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
import com.davemega.hub.avatarforge.AnimationClipPreset
import com.davemega.hub.avatarforge.AvatarCustomizationState
import com.davemega.hub.avatarforge.AvatarForgeEngine
import com.davemega.hub.avatarforge.AvatarForgeSurfaceView
import com.davemega.hub.avatarforge.AvatarRegionNames
import com.davemega.hub.avatarforge.AvatarSpec
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.CameraPreset
import com.davemega.hub.avatarforge.FacialExpressionPreset
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.LightingPreset
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import java.io.File
import kotlin.math.roundToInt
import kotlin.random.Random
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

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
    var customization by remember { mutableStateOf(AvatarCustomizationState()) }
    var generationStatus by remember { mutableStateOf("On-device generation has not been run in this session.") }
    var rendererStatus by remember { mutableStateOf("Waiting for AvatarForge or Filament preview surface.") }
    // Route external animated GLBs to the Filament gltfio preview so skeletal clips can play.
    // AvatarForge-generated avatars, diagnostics, and static GLBs default to AvatarForgeSurfaceView
    // (Vulkan-first with GLES 3.1 fallback) so they do not trigger Filament's morph-storage blockers.
    var useFilamentAnimationPreview by remember(avatarUri, pixelDiagnosticPath) {
        val isActiveDiagnostic = pixelDiagnosticPath != null && avatarUri?.path == pixelDiagnosticPath
        mutableStateOf(!isActiveDiagnostic && GlbPreviewRouter.shouldDefaultToFilament(avatarUri))
    }
    var avatarForgeSurfaceView by remember { mutableStateOf<AvatarForgeSurfaceView?>(null) }
    var selectedCameraPreset by remember(avatarUri) { mutableStateOf(CameraPreset.FULL_BODY) }
    var selectedLightingPreset by remember(avatarUri) { mutableStateOf(LightingPreset.STUDIO) }
    var lightRotationDeg by remember(avatarUri) { mutableStateOf(0f) }
    var liveMicroLife by remember(avatarUri) { mutableStateOf(true) }
    var snapshotStatus by remember(avatarUri) { mutableStateOf("") }
    var autoTurntable by remember(avatarUri) { mutableStateOf(false) }
    var showSkeleton by remember(avatarUri) { mutableStateOf(false) }
    var flatShading by remember(avatarUri) { mutableStateOf(false) }
    var liveJointCount by remember(avatarUri) { mutableStateOf(0) }
    var liveBlendshapeCount by remember(avatarUri) { mutableStateOf(0) }
    var liveRigActive by remember(avatarUri) { mutableStateOf(false) }
    var selectedClip by remember(avatarUri) { mutableStateOf(AnimationClipPreset.REST) }
    var clipSpeed by remember(avatarUri) { mutableStateOf(1.0f) }
    var selectedExpression by remember(avatarUri) { mutableStateOf(FacialExpressionPreset.NEUTRAL) }
    var pickedRegionId by remember(avatarUri) { mutableStateOf(-1) }
    var pickedRegionDistance by remember(avatarUri) { mutableStateOf(0f) }
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
                                outputName = "diagnostic-seed-1337-${System.currentTimeMillis()}.glb",
                                preserveLiveRig = false
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
        Text("External animated GLBs open in the Filament gltfio preview so embedded skeletal clips can play. AvatarForge-generated avatars, diagnostics, and static GLBs open in the native AvatarForge GLES viewport.")

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("3D viewport", style = MaterialTheme.typography.titleMedium)
                key(avatarUri, useFilamentAnimationPreview) {
                    val importedGlb = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)
                    if (importedGlb != null && importedGlb.isFile && !useFilamentAnimationPreview) {
                        val isDiagnosticSession = pixelDiagnosticPath != null && importedGlb.absolutePath == pixelDiagnosticPath
                        AndroidView(
                            factory = { viewContext ->
                                AvatarForgeSurfaceView(
                                    context = viewContext,
                                    glbFile = importedGlb,
                                    verifyVisiblePixels = isDebuggable,
                                    allowLiveRig = !isDiagnosticSession,
                                    onCapabilities = { joints, shapes, isLive ->
                                        liveJointCount = joints
                                        liveBlendshapeCount = shapes
                                        liveRigActive = isLive
                                    },
                                    onRegionPicked = { regionId, distanceM ->
                                        pickedRegionId = regionId
                                        pickedRegionDistance = distanceM
                                    }
                                ) { status ->
                                    android.os.Handler(android.os.Looper.getMainLooper()).post {
                                        rendererStatus = status
                                        val savedPath = context.getSharedPreferences("avatarforge_renderer_smoke", Context.MODE_PRIVATE)
                                            .getString("screenshot_path", null)
                                        diagnosticScreenshot = savedPath?.let(::File)?.takeIf { it.isFile }
                                    }
                                }.also { created ->
                                    avatarForgeSurfaceView = created
                                    created.setLightingPreset(selectedLightingPreset)
                                    created.setSkeletonVisible(showSkeleton)
                                    created.setFlatShading(flatShading)
                                    created.setAutoTurntable(autoTurntable)
                                }
                            },
                            update = { view ->
                                avatarForgeSurfaceView = view
                            },
                            modifier = Modifier.fillMaxWidth().height(420.dp)
                        )
                    } else {
                        AndroidView(
                            factory = { viewContext ->
                                FilamentPreviewView(viewContext).apply {
                                    setStatusListener { rendererStatus = it }
                                    setAnimationInfoListener { count, index, playing, loop, speed, duration, position ->
                                        animationCount = count
                                        selectedAnimation = index
                                        animationPlaying = playing
                                        animationLoop = loop
                                        animationSpeed = speed
                                        animationDuration = duration
                                        animationPosition = position
                                    }
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
                    "Drag to orbit · Pinch to zoom · Tap to inspect region · Double-tap to reset camera",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                if (hasImportedFile && !useFilamentAnimationPreview) {
                    if (pickedRegionId >= 0) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Text(
                                "Selected region: ${AvatarRegionNames.displayName(pickedRegionId)} (${String.format(java.util.Locale.US, "%.2f", pickedRegionDistance)} m)",
                                style = MaterialTheme.typography.labelMedium,
                                fontWeight = FontWeight.SemiBold,
                                color = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.weight(1f)
                            )
                            FilterChip(
                                selected = false,
                                onClick = {
                                    pickedRegionId = -1
                                    pickedRegionDistance = 0f
                                    avatarForgeSurfaceView?.clearRegionHighlight()
                                },
                                label = { Text("Clear") }
                            )
                        }
                    }
                    Text("Camera", style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.SemiBold)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        CameraPreset.entries.forEach { preset ->
                            FilterChip(
                                selected = selectedCameraPreset == preset,
                                onClick = {
                                    selectedCameraPreset = preset
                                    avatarForgeSurfaceView?.setCameraPreset(preset)
                                },
                                label = { Text(preset.label) }
                            )
                        }
                    }
                    Text("Studio lighting", style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.SemiBold)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        LightingPreset.entries.forEach { preset ->
                            FilterChip(
                                selected = selectedLightingPreset == preset,
                                onClick = {
                                    selectedLightingPreset = preset
                                    avatarForgeSurfaceView?.setLightingPreset(preset)
                                },
                                label = { Text(preset.label) }
                            )
                        }
                    }
                    Text(
                        "Key light angle: ${lightRotationDeg.roundToInt()}°",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Slider(
                        value = lightRotationDeg,
                        onValueChange = { deg ->
                            lightRotationDeg = deg
                            avatarForgeSurfaceView?.setLightRotationDeg(deg)
                        },
                        valueRange = 0f..360f
                    )
                    Text("Inspect", style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.SemiBold)
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        if (liveRigActive) {
                            FilterChip(
                                selected = liveMicroLife,
                                onClick = {
                                    liveMicroLife = !liveMicroLife
                                    avatarForgeSurfaceView?.setLiveMicroLife(liveMicroLife)
                                },
                                label = { Text("Alive") }
                            )
                        }
                        FilterChip(
                            selected = autoTurntable,
                            onClick = {
                                autoTurntable = !autoTurntable
                                avatarForgeSurfaceView?.setAutoTurntable(autoTurntable)
                            },
                            label = { Text("Turntable") }
                        )
                        FilterChip(
                            selected = showSkeleton,
                            onClick = {
                                showSkeleton = !showSkeleton
                                avatarForgeSurfaceView?.setSkeletonVisible(showSkeleton)
                            },
                            label = { Text("Skeleton") }
                        )
                        FilterChip(
                            selected = flatShading,
                            onClick = {
                                flatShading = !flatShading
                                avatarForgeSurfaceView?.setFlatShading(flatShading)
                            },
                            label = { Text("Clay") }
                        )
                        FilterChip(
                            selected = false,
                            onClick = {
                                snapshotStatus = "Capturing HD studio snapshot…"
                                avatarForgeSurfaceView?.captureStudioSnapshot { result ->
                                    snapshotStatus = result.fold(
                                        onSuccess = { file -> "HD snapshot saved: ${file.name}" },
                                        onFailure = { err -> "Snapshot failed: ${err.message ?: "unknown error"}" }
                                    )
                                }
                            },
                            label = { Text("HD Photo") }
                        )
                    }
                    if (snapshotStatus.isNotBlank()) {
                        Text(
                            snapshotStatus,
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    if (liveJointCount > 0) {
                        Text(
                            if (liveRigActive) "Animation ($liveJointCount joints · live rig)" else "Animation ($liveJointCount joints)",
                            style = MaterialTheme.typography.labelMedium,
                            fontWeight = FontWeight.SemiBold
                        )
                        Row(
                            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            AnimationClipPreset.entries.forEach { clip ->
                                FilterChip(
                                    selected = selectedClip == clip,
                                    onClick = {
                                        selectedClip = clip
                                        avatarForgeSurfaceView?.setAnimationClip(clip, clipSpeed, true)
                                    },
                                    label = { Text(clip.label) }
                                )
                            }
                        }
                        Row(
                            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            listOf(0.5f to "0.5x", 1.0f to "1x", 1.5f to "1.5x").forEach { (spd, label) ->
                                FilterChip(
                                    selected = kotlin.math.abs(clipSpeed - spd) < 0.05f,
                                    onClick = {
                                        clipSpeed = spd
                                        avatarForgeSurfaceView?.setAnimationClip(selectedClip, spd, true)
                                    },
                                    label = { Text("Speed $label") }
                                )
                            }
                        }
                    }
                    if (liveBlendshapeCount > 0) {
                        Text(
                            "Expressions ($liveBlendshapeCount ARKit shapes)",
                            style = MaterialTheme.typography.labelMedium,
                            fontWeight = FontWeight.SemiBold
                        )
                        Row(
                            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(6.dp)
                        ) {
                            FacialExpressionPreset.entries.forEach { expr ->
                                FilterChip(
                                    selected = selectedExpression == expr,
                                    onClick = {
                                        selectedExpression = expr
                                        avatarForgeSurfaceView?.setFacialExpression(expr)
                                    },
                                    label = { Text(expr.label) }
                                )
                            }
                        }
                    }
                }
                Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
                if (hasImportedFile) {
                    OutlinedButton(
                        onClick = {
                            useFilamentAnimationPreview = !useFilamentAnimationPreview
                            if (!useFilamentAnimationPreview) {
                                filamentPreviewView = null
                                animationCount = 0
                                animationPosition = 0f
                            } else {
                                avatarForgeSurfaceView = null
                            }
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (useFilamentAnimationPreview) "Use AvatarForge native preview" else "Open animation-capable Filament preview")
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
                        Text("Waiting for GLB animation tracks. If this reports zero animations, the file has no playable clip for Filament.")
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
                Text("AvatarForge engine", style = MaterialTheme.typography.titleMedium)
                if (engineHealth.usable) {
                    Text("Native library loaded · ABI ${engineHealth.abiVersion} · ${engineHealth.engineVersion}")
                    Text("The native engine is loaded; generation and rendering still require a real phone run.", style = MaterialTheme.typography.bodySmall)
                } else {
                    Text("UNAVAILABLE: ${engineHealth.problem ?: "native engine health check failed"}")
                }
                Text("Sex", style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.SemiBold)
                Row(
                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    listOf(Sex.FEMALE, Sex.MALE, Sex.UNSPECIFIED).forEach { option ->
                        FilterChip(
                            selected = customization.sex == option,
                            onClick = { customization = customization.copy(sex = option) },
                            label = { Text(AvatarCustomizationState.displayName(option)) }
                        )
                    }
                }
                Text("Build", style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.SemiBold)
                Row(
                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Build.entries.forEach { option ->
                        FilterChip(
                            selected = customization.build == option,
                            onClick = { customization = customization.copy(build = option) },
                            label = { Text(AvatarCustomizationState.displayName(option)) }
                        )
                    }
                }
                Text("Hairstyle (3D strand cards)", style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.SemiBold)
                Row(
                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    HairStyle.entries.forEach { option ->
                        FilterChip(
                            selected = customization.hairStyle == option,
                            onClick = { customization = customization.copy(hairStyle = option) },
                            label = { Text(AvatarCustomizationState.displayName(option)) }
                        )
                    }
                }
                Text("Outfit", style = MaterialTheme.typography.labelMedium, fontWeight = FontWeight.SemiBold)
                Row(
                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    AvatarCustomizationState.SELECTABLE_OUTFITS.forEach { option ->
                        FilterChip(
                            selected = customization.outfit == option,
                            onClick = { customization = customization.copy(outfit = option) },
                            label = { Text(AvatarCustomizationState.displayName(option)) }
                        )
                    }
                }
                Text("Height: ${customization.heightCm} cm", style = MaterialTheme.typography.labelMedium)
                Slider(
                    value = customization.heightCm.toFloat(),
                    onValueChange = { customization = customization.copy(heightCm = it.roundToInt()) },
                    valueRange = 150f..205f
                )
                Text(
                    "Skin tone melanin: ${(customization.skinMelanin * 100f).roundToInt()}%",
                    style = MaterialTheme.typography.labelMedium
                )
                Slider(
                    value = customization.skinMelanin,
                    onValueChange = { customization = customization.copy(skinMelanin = it) },
                    valueRange = 0.08f..0.90f
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(
                        "Seed: ${customization.seed}",
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.weight(1f).padding(top = 10.dp)
                    )
                    OutlinedButton(
                        onClick = { customization = customization.copy(seed = Random.nextInt(100, 999_999)) },
                        enabled = !generating
                    ) {
                        Text("Randomize seed")
                    }
                }
                OutlinedTextField(
                    value = customization.customNotes,
                    onValueChange = { customization = customization.copy(customNotes = it.take(300)) },
                    label = { Text("Extra style or color notes (optional)") },
                    placeholder = { Text("e.g. auburn hair, dark navy jacket, emerald eyes") },
                    modifier = Modifier.fillMaxWidth(),
                    minLines = 2
                )
                Text(
                    "Spec prompt: ${customization.buildPrompt()}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Button(
                    onClick = {
                        generationJob = scope.launch {
                            generationStatus = "Running native generation on this device. Resource and thermal gates are checked before work begins."
                            try {
                                pixelDiagnosticPath = null
                                val spec = customization.toAvatarSpec()
                                val result = AvatarForgeEngine.generate(
                                    context = context,
                                    spec = spec,
                                    prompt = spec.prompt
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
                    enabled = engineHealth.usable && !generating && !isImporting,
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

