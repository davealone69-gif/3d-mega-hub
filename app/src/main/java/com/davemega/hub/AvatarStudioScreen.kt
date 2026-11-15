package com.davemega.hub

import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.net.Uri
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.key
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.FileProvider
import androidx.compose.ui.viewinterop.AndroidView
import com.davemega.hub.avatarforge.AvatarForgeEngine
import com.davemega.hub.avatarforge.AvatarForgeSurfaceView
import com.davemega.hub.avatarforge.AvatarSpec
import com.davemega.hub.avatarforge.EvidenceIdentity
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import java.io.File
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

/**
 * The avatar creator.
 *
 * Laid out after the "Create Your Avatar" guide: the avatar on a stage, a rail of sections beside
 * it, the chosen section's options underneath as pictures, colour dots and sliders, and three
 * actions - Randomize, Save, Create. The sections are the ones the engine has options for.
 *
 * The choices themselves follow the owner's second reference, a dark picture creator: ethnicity;
 * body type, breast size and butt size; hair colour and hair style - each as pictures with the
 * name written over them. Its dark look is the default here and the header switches to the light
 * look of the first guide.
 *
 * An avatar made here is drawn by the AvatarForge GLES renderer, which also plays its animations:
 * the strip under the stage starts them and the Motion section has the rest of the controls. The
 * Filament preview is kept for files from elsewhere that carry their own clips, and is never
 * given a file it cannot hold (see [GlbPreviewRouting]).
 */
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
    var generationStatus by remember { mutableStateOf("Pick a look, then press Create avatar.") }
    var rendererStatus by remember { mutableStateOf("Waiting for the 3D view.") }
    // Measured values published by the native GLES render thread; empty until a frame was measured.
    var telemetryLines by remember { mutableStateOf<List<String>>(emptyList()) }
    val importedFile = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)?.takeIf { it.isFile }
    val fileSummary = remember(avatarUri) { importedFile?.let { GlbPreviewRouting.summarize(it) } }
    // A file from elsewhere that carries clips opens in the gltfio preview, which plays them. An
    // avatar made here stays on the GLES renderer, which animates it itself; gltfio cannot hold it.
    var useFilamentAnimationPreview by remember(avatarUri) {
        mutableStateOf(GlbPreviewRouting.opensInAnimationPreview(fileSummary))
    }
    var filamentPreviewView by remember { mutableStateOf<FilamentPreviewView?>(null) }
    var animationCount by remember { mutableStateOf(0) }
    var selectedAnimation by remember { mutableStateOf(0) }
    var animationPlaying by remember { mutableStateOf(false) }
    var animationLoop by remember { mutableStateOf(true) }
    var animationSpeed by remember { mutableStateOf(1.0f) }
    var animationDuration by remember { mutableStateOf(0.0f) }
    var animationPosition by remember { mutableStateOf(0.0f) }
    // The built-in animations of an avatar shown by the GLES renderer.
    var avatarView by remember { mutableStateOf<AvatarForgeSurfaceView?>(null) }
    var avatarClips by remember(avatarUri) { mutableStateOf<List<AvatarForgeSurfaceView.AnimationClip>>(emptyList()) }
    var avatarClip by remember(avatarUri) { mutableStateOf(AvatarForgeSurfaceView.NO_ANIMATION) }
    var avatarPaused by remember(avatarUri) { mutableStateOf(false) }
    var avatarLoop by remember(avatarUri) { mutableStateOf(true) }
    var avatarSpeed by remember(avatarUri) { mutableStateOf(1.0f) }
    var generationJob by remember { mutableStateOf<Job?>(null) }
    var appearance by remember { mutableStateOf(AppearanceSelection()) }
    var category by remember { mutableStateOf(CreatorCategory.LOOK) }
    val creatorPrefs = remember { context.getSharedPreferences("avatar_creator", Context.MODE_PRIVATE) }
    var darkCreator by remember { mutableStateOf(creatorPrefs.getBoolean("dark_look", true)) }
    var showAdvanced by remember { mutableStateOf(false) }
    val generating = generationJob?.isActive == true
    val controlsEnabled = !generating && !isImporting

    fun playAvatarClip(index: Int) {
        avatarClip = index
        avatarPaused = false
        avatarView?.playAnimation(index)
    }

    MaterialTheme(colorScheme = if (darkCreator) CreatorColoursDark else CreatorColours) {
        // Text that names no colour takes the content colour, which comes from the app's own theme
        // outside this screen; on the dark look that would be dark text on a dark sheet.
        CompositionLocalProvider(LocalContentColor provides MaterialTheme.colorScheme.onBackground) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(28.dp))
                .background(if (darkCreator) CreatorBackdropDark else CreatorBackdrop)
                .padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    "Create Your Avatar",
                    style = MaterialTheme.typography.headlineSmall,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onBackground,
                    modifier = Modifier.weight(1f)
                )
                CreatorChip(if (darkCreator) "\u2600 Light" else "\u263E Dark", selected = false) {
                    darkCreator = !darkCreator
                    creatorPrefs.edit().putBoolean("dark_look", darkCreator).apply()
                }
            }
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
                                        outfit = Outfit.TSHIRT,
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

            /* ------------------------------------------------------------ stage and rail ---- */
            Row(
                modifier = Modifier.fillMaxWidth().height(440.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxHeight()
                        .background(CreatorStage, RoundedCornerShape(20.dp))
                        .padding(6.dp)
                ) {
                    key(avatarUri, useFilamentAnimationPreview) {
                        if (importedFile != null && !useFilamentAnimationPreview) {
                            // A render session publishes one last snapshot while it shuts down. Once this
                            // viewport has left the screen those numbers describe a renderer that is no
                            // longer shown, so they must not be displayed under a different model.
                            val viewportActive = remember { java.util.concurrent.atomic.AtomicBoolean(true) }
                            DisposableEffect(Unit) {
                                onDispose {
                                    viewportActive.set(false)
                                    telemetryLines = emptyList()
                                    avatarView = null
                                }
                            }
                            AndroidView(
                                factory = { viewContext ->
                                    AvatarForgeSurfaceView(
                                        viewContext,
                                        importedFile,
                                        verifyVisiblePixels = isDebuggable,
                                        onTelemetry = { snapshot ->
                                            // Runs on the render thread; the build and asset digests
                                            // were computed there when the session started.
                                            val lines = snapshot.summaryLines() +
                                                EvidenceIdentity.summaryLines(viewContext, importedFile)
                                            android.os.Handler(android.os.Looper.getMainLooper()).post {
                                                if (viewportActive.get()) telemetryLines = lines
                                            }
                                        }
                                    ) { status ->
                                        android.os.Handler(android.os.Looper.getMainLooper()).post {
                                            rendererStatus = status
                                            val savedPath = context.getSharedPreferences("avatarforge_renderer_smoke", Context.MODE_PRIVATE)
                                                .getString("screenshot_path", null)
                                            diagnosticScreenshot = savedPath?.let(::File)?.takeIf { it.isFile }
                                        }
                                    }.also { view ->
                                        view.setAnimationListener { clips ->
                                            if (viewportActive.get()) avatarClips = clips
                                        }
                                        avatarView = view
                                    }
                                },
                                modifier = Modifier.fillMaxSize()
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
                                modifier = Modifier.fillMaxSize()
                            )
                        }
                    }
                    if (importedFile != null && !useFilamentAnimationPreview) {
                        // The guide's turn and zoom buttons, over the stage.
                        Column(
                            modifier = Modifier.align(Alignment.CenterStart).padding(4.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            StageButton("\u21BB") { avatarView?.turnBy(45f) }
                            StageButton("+") { avatarView?.zoomBy(0.8f) }
                            StageButton("\u2212") { avatarView?.zoomBy(1.25f) }
                            StageButton("\u2302") { avatarView?.resetView() }
                        }
                    }
                }
                CreatorRail(
                    selected = category,
                    onSelect = { category = it },
                    modifier = Modifier.width(78.dp).fillMaxHeight()
                )
            }
            Text(
                "Drag to turn · pinch to zoom · double-tap to reset the view",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            /* ------------------------------------------------------------ animation strip --- */
            if (avatarClips.isNotEmpty() && !useFilamentAnimationPreview) {
                Row(
                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    CreatorChip("Stand", avatarClip == AvatarForgeSurfaceView.NO_ANIMATION) {
                        playAvatarClip(AvatarForgeSurfaceView.NO_ANIMATION)
                    }
                    avatarClips.forEach { clip ->
                        CreatorChip(CreatorLabels.animation(clip.name), avatarClip == clip.index) { playAvatarClip(clip.index) }
                    }
                }
            }

            /* ------------------------------------------------------------ section panel ------ */
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(24.dp))
                    .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.92f))
                    .padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                when (category) {
                    CreatorCategory.LOOK -> LookPanel(appearance, controlsEnabled) { appearance = it }
                    CreatorCategory.HAIR -> HairPanel(appearance, controlsEnabled) { appearance = it }
                    CreatorCategory.BODY -> BodyPanel(appearance, controlsEnabled) { appearance = it }
                    CreatorCategory.FACE -> FacePanel(appearance, controlsEnabled) { appearance = it }
                    CreatorCategory.SKIN -> SkinPanel(appearance, controlsEnabled) { appearance = it }
                    CreatorCategory.OUTFIT -> OutfitPanel(appearance, controlsEnabled) { appearance = it }
                    CreatorCategory.SHOES -> ShoesPanel(appearance, controlsEnabled) { appearance = it }
                    CreatorCategory.MOTION -> {
                        Text("Motion", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                        if (useFilamentAnimationPreview) {
                            if (animationCount > 0) {
                                Text("Animations in this file · ${selectedAnimation + 1} of $animationCount · ${"%.2f".format(animationDuration)} s")
                                Row(
                                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
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
                                Row(
                                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    CreatorChip(if (animationLoop) "Loop: on" else "Loop: off", animationLoop) {
                                        animationLoop = !animationLoop
                                        filamentPreviewView?.setAnimationLoop(animationLoop)
                                    }
                                    listOf(0.5f, 1.0f, 1.5f, 2.0f).forEach { speed ->
                                        CreatorChip("${speed}x", animationSpeed == speed) {
                                            animationSpeed = speed
                                            filamentPreviewView?.setAnimationSpeed(speed)
                                        }
                                    }
                                }
                            } else {
                                Text(
                                    "Waiting for this file's animation tracks. If none appear, the file has no clip this preview can play.",
                                    style = MaterialTheme.typography.bodySmall
                                )
                            }
                        } else if (avatarClips.isNotEmpty()) {
                            Text(
                                "${avatarClips.size} animations are built into this avatar. They play in the view above.",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                            Row(
                                modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                                horizontalArrangement = Arrangement.spacedBy(8.dp)
                            ) {
                                CreatorChip("Stand", avatarClip == AvatarForgeSurfaceView.NO_ANIMATION) {
                                    playAvatarClip(AvatarForgeSurfaceView.NO_ANIMATION)
                                }
                                avatarClips.forEach { clip ->
                                    CreatorChip(
                                        "${CreatorLabels.animation(clip.name)} · ${"%.1f".format(clip.durationSeconds)} s",
                                        avatarClip == clip.index
                                    ) { playAvatarClip(clip.index) }
                                }
                            }
                            Row(
                                modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                                horizontalArrangement = Arrangement.spacedBy(8.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Button(
                                    onClick = {
                                        if (avatarClip == AvatarForgeSurfaceView.NO_ANIMATION) {
                                            playAvatarClip(avatarClips.first().index)
                                        } else {
                                            avatarPaused = !avatarPaused
                                            avatarView?.setAnimationPaused(avatarPaused)
                                        }
                                    }
                                ) {
                                    Text(if (avatarClip != AvatarForgeSurfaceView.NO_ANIMATION && !avatarPaused) "Pause" else "Play")
                                }
                                OutlinedButton(onClick = { playAvatarClip(AvatarForgeSurfaceView.NO_ANIMATION) }) { Text("Stop") }
                                CreatorChip(if (avatarLoop) "Loop: on" else "Loop: off", avatarLoop) {
                                    avatarLoop = !avatarLoop
                                    avatarView?.setAnimationLoop(avatarLoop)
                                }
                            }
                            Row(
                                modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                                horizontalArrangement = Arrangement.spacedBy(8.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text("Speed", style = MaterialTheme.typography.labelLarge)
                                listOf(0.5f, 1.0f, 1.5f, 2.0f).forEach { speed ->
                                    CreatorChip("${speed}x", avatarSpeed == speed) {
                                        avatarSpeed = speed
                                        avatarView?.setAnimationSpeed(speed)
                                    }
                                }
                            }
                        } else {
                            Text(
                                if (importedFile == null) {
                                    "Create an avatar first. Every avatar made here comes with its animations: idle, walk, run, jump, wave and more."
                                } else {
                                    "This model has no animation the app can play. Avatars made here have them built in; a file from elsewhere plays its own clips if it carries any."
                                },
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }

            /* ------------------------------------------------------------ actions ------------ */
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                OutlinedButton(
                    onClick = { appearance = randomAppearance(appearance) },
                    enabled = controlsEnabled,
                    modifier = Modifier.weight(1f)
                ) { Text("Randomize", maxLines = 1) }
                OutlinedButton(
                    onClick = {
                        runCatching {
                            val source = requireNotNull(importedFile) { "There is no avatar file to save yet." }
                            val uri = FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", source)
                            val shareIntent = Intent(Intent.ACTION_SEND).apply {
                                type = "model/gltf-binary"
                                putExtra(Intent.EXTRA_STREAM, uri)
                                clipData = ClipData.newUri(context.contentResolver, source.name, uri)
                                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                            }
                            context.startActivity(Intent.createChooser(shareIntent, "Save or share the avatar (.glb)"))
                        }.onFailure { failure ->
                            generationStatus = "Could not save the avatar: ${failure.message ?: failure.javaClass.simpleName}"
                        }
                    },
                    enabled = importedFile != null && controlsEnabled,
                    modifier = Modifier.weight(1f)
                ) { Text("Save", maxLines = 1) }
            }
            Button(
                onClick = {
                    val chosen = appearance
                    generationJob = scope.launch {
                        generationStatus = "Creating: ${chosen.summary()}."
                        try {
                            pixelDiagnosticPath = null
                            val result = AvatarForgeEngine.generate(
                                context = context,
                                spec = chosen.toSpec(),
                                prompt = AppearanceCatalogue.PRESET_PROMPT
                            )
                            onGenerated(Uri.fromFile(result.file), result.file.name)
                            generationStatus = "Created (${chosen.summary()}): ${result.triangleCount} triangles · " +
                                "${result.vertexCount} vertices · ${result.jointCount} joints."
                        } catch (cancelled: CancellationException) {
                            generationStatus = "Cancelled."
                        } catch (failure: Exception) {
                            generationStatus = "Could not create the avatar: ${failure.message ?: failure.javaClass.simpleName}"
                        } finally {
                            generationJob = null
                        }
                    }
                },
                enabled = engineHealth.usable && controlsEnabled,
                modifier = Modifier.fillMaxWidth().height(54.dp)
            ) {
                Text(if (generating) "Creating…" else "Create avatar  \u2192", fontWeight = FontWeight.Bold, fontSize = 16.sp)
            }
            if (generating) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(modifier = Modifier.size(28.dp))
                    OutlinedButton(onClick = { generationJob?.cancel() }) { Text("Cancel") }
                }
            }
            Text(generationStatus, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onBackground)
            if (!engineHealth.usable) {
                Text(
                    "UNAVAILABLE: ${engineHealth.problem ?: "native engine health check failed"}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error
                )
            }

            /* ------------------------------------------------------------ everything else ---- */
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(16.dp))
                    .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.7f))
                    .clickable { showAdvanced = !showAdvanced }
                    .padding(horizontal = 14.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    "Import, describe in words, diagnostics",
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.weight(1f)
                )
                Text(if (showAdvanced) "\u25B2" else "\u25BC")
            }
            if (showAdvanced) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(24.dp))
                        .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.92f))
                        .padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text("Model", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    Text("Selected: $avatarName", style = MaterialTheme.typography.bodySmall)
                    if (importStatus.isNotBlank()) {
                        Text(importStatus, style = MaterialTheme.typography.bodySmall)
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = onImport, enabled = controlsEnabled) {
                            Text(if (isImporting) "Importing…" else "Import GLB")
                        }
                        if (avatarUri != null) OutlinedButton(onClick = onRemove, enabled = controlsEnabled) { Text("Use sample model") }
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
                        enabled = controlsEnabled && remoteGlbUrl.isNotBlank(),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (isImporting) "Downloading GLB…" else "Download & Render URL")
                    }
                    if (importedFile != null) {
                        val refusal = GlbPreviewRouting.animationPreviewRefusal(fileSummary)
                        if (GlbPreviewRouting.mayOfferAnimationPreview(fileSummary)) {
                            OutlinedButton(
                                onClick = {
                                    useFilamentAnimationPreview = !useFilamentAnimationPreview
                                    if (!useFilamentAnimationPreview) {
                                        filamentPreviewView = null
                                        animationCount = 0
                                        animationPosition = 0f
                                    }
                                },
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Text(if (useFilamentAnimationPreview) "Use AvatarForge GLES preview" else "Open animation-capable Filament preview")
                            }
                        } else if (refusal != null) {
                            Text(
                                "The Filament animation preview is not offered for this file: $refusal.",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }

                    Text("Describe it in words", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    OutlinedTextField(
                        value = generationPrompt,
                        onValueChange = { generationPrompt = it.take(500) },
                        label = { Text("Describe your avatar") },
                        modifier = Modifier.fillMaxWidth(),
                        minLines = 3
                    )
                    Text(
                        "Generation uses AvatarForge's deterministic prompt/measurement parser. It is not photo-to-identity reconstruction or an embedded LLM.",
                        style = MaterialTheme.typography.bodySmall
                    )
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
                    Text("Renderer", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    if (engineHealth.usable) {
                        Text("Native library loaded · ABI ${engineHealth.abiVersion} · ${engineHealth.engineVersion}", style = MaterialTheme.typography.bodySmall)
                    }
                    Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
                    if (telemetryLines.isNotEmpty()) {
                        Text("Measured renderer diagnostics", style = MaterialTheme.typography.titleSmall)
                        telemetryLines.forEach { line ->
                            Text(line, style = MaterialTheme.typography.bodySmall)
                        }
                        Text(
                            "Measured on this device by the render thread and updated about once per second. " +
                                "GPU byte figures are the sizes requested from the driver.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    Text(
                        "Renderer reports are saved to Downloads/3D Mega Hub Diagnostics: renderer_diagnostics.json when a " +
                            "renderer check runs, and renderer_telemetry.json with the final measured numbers when this viewport closes.",
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
                }
            }
        }
        }
    }
}
