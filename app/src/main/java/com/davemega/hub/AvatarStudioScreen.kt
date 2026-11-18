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
import com.davemega.hub.avatarforge.AnimationEngine
import com.davemega.hub.avatarforge.AvatarForgeEngine
import com.davemega.hub.avatarforge.FacialExpression
import com.davemega.hub.avatarforge.AvatarForgeSurfaceView
import com.davemega.hub.avatarforge.AvatarSpec
import com.davemega.hub.avatarforge.Build
import com.davemega.hub.avatarforge.HairStyle
import com.davemega.hub.avatarforge.Outfit
import com.davemega.hub.avatarforge.Sex
import com.davemega.hub.avatarforge.GlbValidator
import java.io.File
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

/* glTF extensions that the pinned Filament 1.71.6 gltfio cannot load without aborting the process.
   Measured, not guessed: a generated avatar declares KHR_materials_sheen on 10 of its 13 materials,
   and handing that file to gltfio produced

     utils::PreconditionPanic: Precondition in getFieldInfo:169
     reason: uniform named "sheenColorIndex" not found

   as an uncaught C++ exception in libfilament-jni.so - SIGABRT on the main thread, which no Kotlin
   runCatching can intercept. Anything listed here must not be routed to the Filament preview. */
private val FILAMENT_UNSUPPORTED_EXTENSIONS = setOf("KHR_materials_sheen")

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
    /* File-backed avatars - imported or generated - render through the native AvatarForge GLES route,
       so this defaults to false. It used to default to true for any file URI, on the stated grounds
       that "the native AvatarForge GLES renderer currently does not skin imported GLBs". That stopped
       being true when the import path began adopting a file's own skeleton and driving it through
       af::anim::Player, with nativeRender advancing the imported clock before each draw.

       The stale default had two consequences, and the second is a crash:

         1. The native route never ran for a file, so its diagnostics - presented_frames, triangles,
            draw_calls, gl_error, egl_error, pixel_count, bounds_in_frustum, screenshot_path - were
            never written. The instrumented smoke test asserts on exactly those, so it could not pass
            no matter how well the renderer worked.
         2. gltfio took the file instead and aborted the process on an extension our own writer emits:
            "terminating due to uncaught exception of type utils::PreconditionPanic: Precondition in
            getFieldInfo:169 reason: uniform named \"sheenColorIndex\" not found", an uncaught C++
            exception in libfilament-jni.so, SIGABRT on the main thread. 10 of the 13 materials in a
            generated avatar declare KHR_materials_sheen, and the pinned Filament 1.71.6 ubershader has
            no such uniform. The native renderer implements that sheen lobe itself (Charlie
            distribution, af_gl.cpp), so routing here is also the higher-fidelity choice.

       The bundled sample still uses Filament: with no file URI there is no importedGlb, so it takes the
       preview branch whatever this flag says. The toggle below keeps the Filament route reachable for
       any file, so both renderers remain usable and neither is being retired. */
    var useFilamentAnimationPreview by remember(avatarUri) { mutableStateOf(false) }
    /* Extensions the pinned Filament 1.71.6 gltfio cannot survive. KHR_materials_sheen makes gltfio
       set a "sheenColorIndex" uniform the ubershader does not declare, and Filament turns that into an
       uncaught utils::PreconditionPanic - SIGABRT on the main thread, not a catchable Kotlin error, so
       there is nothing to recover from and the only defence is not to offer the route. The native
       AvatarForge GLES renderer implements the sheen lobe itself, so this is a limitation of the pinned
       Filament build rather than a missing feature in the avatar. */
    val filamentBlockedExtensions = remember(avatarUri) {
        val file = avatarUri?.takeIf { it.scheme == "file" }?.path?.let(::File)
        if (file == null) emptySet()
        else GlbValidator.extensionsUsed(file) intersect FILAMENT_UNSUPPORTED_EXTENSIONS
    }
    var filamentPreviewView by remember { mutableStateOf<FilamentPreviewView?>(null) }
    var animationCount by remember { mutableStateOf(0) }
    var selectedAnimation by remember { mutableStateOf(0) }
    var animationPlaying by remember { mutableStateOf(false) }
    var animationLoop by remember { mutableStateOf(true) }
    var animationSpeed by remember { mutableStateOf(1.0f) }
    var animationDuration by remember { mutableStateOf(0.0f) }
    var animationPosition by remember { mutableStateOf(0.0f) }

    /* Native animation core controls. These drive af::anim through NativeAnimationDriver; when the core
       cannot attach, the view falls back to gltfio's own Animator and every control below says so rather
       than appearing to work. */
    var nativeAnimEnabled by remember(avatarUri) { mutableStateOf(true) }
    var nativeAnimStatus by remember { mutableStateOf("Native animation core idle.") }
    var nativeClipNames by remember(avatarUri) { mutableStateOf<List<String>>(emptyList()) }
    var nativeClipPlayable by remember(avatarUri) { mutableStateOf<List<Boolean>>(emptyList()) }
    var crossfadeSeconds by remember { mutableStateOf(0.4f) }
    var expressionAmount by remember { mutableStateOf(0.7f) }
    var blinkEnabled by remember { mutableStateOf(false) }
    var lipSyncText by remember { mutableStateOf("Hello, this lip sync is approximate.") }
    var lipSyncSeconds by remember { mutableStateOf(3.0f) }
    var stateMachineBuilt by remember(avatarUri) { mutableStateOf(false) }
    var activeLayerCount by remember(avatarUri) { mutableStateOf(0) }
    var advanceMicros by remember { mutableStateOf(0f) }
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
        Text("Imported and generated GLBs render through the native AvatarForge GLES path, which skins the file's own skeleton and plays its clips. Switch to the Filament gltfio preview to compare against gltfio's own sampler.")

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
                                    setAnimationInfoListener { count, index, playing, loop, speed, duration, position ->
                                        animationCount = count
                                        selectedAnimation = index
                                        animationPlaying = playing
                                        animationLoop = loop
                                        animationSpeed = speed
                                        animationDuration = duration
                                        animationPosition = position
                                        /* The frame callback is the only place that knows what the core
                                           is doing right now, so the diagnostics are refreshed from it. */
                                        val view = filamentPreviewView
                                        val driver = view?.nativeAnimationDriver()
                                        if (view != null) nativeAnimStatus = view.nativeAnimationStatus
                                        if (driver != null) {
                                            advanceMicros = driver.advanceMicros()
                                            activeLayerCount = driver.session()?.layerCount ?: 0
                                        }
                                    }
                                    setNativeAnimationEnabled(nativeAnimEnabled)
                                    setModel(avatarUri)
                                    filamentPreviewView = this
                                }
                            },
                            update = { view ->
                                filamentPreviewView = view
                                /* setModel is idempotent for an unchanged URI, so recomposition does not
                                   reload the asset; setNativeAnimationEnabled attaches or detaches in
                                   place when the toggle changes. */
                                view.setNativeAnimationEnabled(nativeAnimEnabled)
                                view.setModel(avatarUri)
                            },
                            modifier = Modifier.fillMaxWidth().height(420.dp)
                        )
                    }
                }
                Text(
                    "Drag to orbit · Pinch to zoom · Double-tap to reset camera",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Text(rendererStatus, style = MaterialTheme.typography.bodySmall)
                if (hasImportedFile) {
                    OutlinedButton(
                        onClick = {
                            useFilamentAnimationPreview = !useFilamentAnimationPreview
                            if (!useFilamentAnimationPreview) {
                                filamentPreviewView = null
                                animationCount = 0
                                animationPosition = 0f
                            }
                        },
                        /* Disabled rather than allowed to crash: switching a sheen-bearing file to
                           gltfio aborts the process, and a button that kills the app is not a control. */
                        enabled = filamentBlockedExtensions.isEmpty(),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (useFilamentAnimationPreview) "Use AvatarForge GLES preview" else "Open Filament gltfio preview")
                    }
                    if (filamentBlockedExtensions.isNotEmpty()) {
                        Text(
                            "Filament preview is unavailable for this file: it declares " +
                                filamentBlockedExtensions.sorted().joinToString(", ") +
                                ", and the pinned Filament 1.71.6 gltfio aborts the process on that " +
                                "extension (uniform \"sheenColorIndex\" not found). The AvatarForge " +
                                "GLES renderer implements it and is being used instead.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
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

                    /* ================= native animation core ================= */
                    OutlinedButton(
                        onClick = { nativeAnimEnabled = !nativeAnimEnabled },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (nativeAnimEnabled) "Native animation core: on" else "Native animation core: off")
                    }
                    Text(
                        if (nativeAnimEnabled) nativeAnimStatus
                        else "Native animation core is off. gltfio's own Animator drives playback, so crossfades, " +
                            "layers, facial control, lip-sync and retargeting are unavailable.",
                        style = MaterialTheme.typography.bodySmall
                    )
                    val driver = filamentPreviewView?.nativeAnimationDriver()
                    if (nativeAnimEnabled && driver == null) {
                        Text(
                            "The core is not driving this asset yet. It attaches when a GLB finishes loading, and it " +
                                "needs a file with a skeleton or morph targets. Filament's Animator keeps playing the " +
                                "asset meanwhile, so nothing above stops working.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    if (nativeAnimEnabled && driver != null) {
                        val session = driver.session()
                        val playable = driver.playableClips()
                        val coreHealth = AnimationEngine.health

                        Text(
                            "Core: ${playable.size} clip(s) · ${session?.jointCount ?: 0} joints · " +
                                "${session?.rolesMatched ?: 0}/17 roles · ${session?.morphCount ?: 0} morph target(s) · " +
                                "last advance ${"%.1f".format(advanceMicros)} µs",
                            style = MaterialTheme.typography.bodySmall
                        )
                        if (!coreHealth.usable) {
                            Text(
                                "Native core unavailable: ${coreHealth.problem}",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.error
                            )
                        }
                        driver.limitations.forEach { limit ->
                            Text(
                                "· $limit",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }

                        /* ---- clips: play, or crossfade ---- */
                        Text("Clips", style = MaterialTheme.typography.titleSmall)
                        playable.chunked(3).forEach { rowClips ->
                            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                rowClips.forEach { clip ->
                                    OutlinedButton(
                                        onClick = {
                                            driver.play(clip.info.index)
                                            nativeAnimStatus = driver.status
                                        },
                                        enabled = clip.playable,
                                        modifier = Modifier.weight(1f)
                                    ) {
                                        Text(
                                            if (clip.playable) clip.info.name else "${clip.info.name} (n/a)",
                                            style = MaterialTheme.typography.labelSmall
                                        )
                                    }
                                }
                            }
                        }
                        val notPlayable = playable.filterNot { it.playable }
                        if (notPlayable.isNotEmpty()) {
                            Text(
                                "${notPlayable.size} clip(s) the core loaded but this renderer cannot apply: " +
                                    notPlayable.joinToString(", ") { it.info.name },
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }

                        Text(
                            "Crossfade · ${"%.2f".format(crossfadeSeconds)} s",
                            style = MaterialTheme.typography.titleSmall
                        )
                        Slider(
                            value = crossfadeSeconds,
                            onValueChange = { crossfadeSeconds = it },
                            valueRange = 0.05f..2.0f
                        )
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            playable.filter { it.playable }.take(4).forEach { clip ->
                                OutlinedButton(
                                    onClick = {
                                        driver.crossfadeTo(clip.info.index, crossfadeSeconds)
                                        nativeAnimStatus = driver.status
                                    },
                                    modifier = Modifier.weight(1f)
                                ) {
                                    Text("fade → ${clip.info.name}", style = MaterialTheme.typography.labelSmall)
                                }
                            }
                        }

                        /* ---- state machine ---- */
                        Text("State machine", style = MaterialTheme.typography.titleSmall)
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            OutlinedButton(
                                onClick = {
                                    val states = session?.buildStateMachine(crossfadeSeconds, allToAll = true) ?: -1
                                    stateMachineBuilt = states > 0
                                    session?.setStateMachineDriven(stateMachineBuilt)
                                    nativeAnimStatus = if (stateMachineBuilt) {
                                        "State machine built with $states state(s) and every edge declared; " +
                                            "the core now owns the clocks."
                                    } else {
                                        "The state machine was not built: ${session?.lastError() ?: "no clips"}"
                                    }
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text(if (stateMachineBuilt) "Rebuild states" else "Build states") }
                            OutlinedButton(
                                onClick = {
                                    session?.setStateMachineDriven(false)
                                    stateMachineBuilt = false
                                    nativeAnimStatus = "State machine released; direct playback owns the clocks again."
                                },
                                enabled = stateMachineBuilt,
                                modifier = Modifier.weight(1f)
                            ) { Text("Release") }
                        }
                        if (stateMachineBuilt) {
                            playable.filter { it.playable }.chunked(3).forEach { rowClips ->
                                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                    rowClips.forEach { clip ->
                                        OutlinedButton(
                                            onClick = {
                                                /* requestState refuses an undeclared edge instead of
                                                   snapping, and the refusal is reported: a button that
                                                   silently did nothing would look like a bug. */
                                                val accepted = session?.requestState(clip.info.name) ?: false
                                                nativeAnimStatus = if (accepted) {
                                                    "State '${clip.info.name}' requested."
                                                } else {
                                                    "Refused: ${session?.lastError() ?: "unknown state"}"
                                                }
                                            },
                                            modifier = Modifier.weight(1f)
                                        ) {
                                            Text("→ ${clip.info.name}", style = MaterialTheme.typography.labelSmall)
                                        }
                                    }
                                }
                            }
                            Text(
                                "Current state: ${session?.currentStateName ?: "<none>"}",
                                style = MaterialTheme.typography.bodySmall
                            )
                        }

                        /* ---- layers ---- */
                        Text("Layers · $activeLayerCount active", style = MaterialTheme.typography.titleSmall)
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            OutlinedButton(
                                onClick = {
                                    val target = playable.firstOrNull { it.playable }
                                    if (target == null || session == null) {
                                        nativeAnimStatus = "No playable clip to put on a layer."
                                    } else {
                                        val layer = session.addLayer(target.info.index, 0.6f, additive = false)
                                        val masked = if (layer >= 0) {
                                            session.setLayerMask(
                                                layer,
                                                com.davemega.hub.avatarforge.BoneMaskPreset.UPPER_BODY
                                            )
                                        } else -1
                                        activeLayerCount = session.layerCount
                                        nativeAnimStatus = if (layer >= 0) {
                                            "Layer $layer added with '${target.info.name}' at weight 0.6, masked to " +
                                                "$masked upper-body joint(s)."
                                        } else {
                                            "The layer was refused: ${session.lastError()}"
                                        }
                                    }
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text("Add upper-body layer") }
                            OutlinedButton(
                                onClick = {
                                    val count = session?.layerCount ?: 0
                                    if (session != null && count > 0) {
                                        val enabled = session.setLayerEnabled(count - 1, false)
                                        nativeAnimStatus = if (enabled) "Layer ${count - 1} muted."
                                        else "Layer ${count - 1} could not be muted."
                                    } else {
                                        nativeAnimStatus = "There is no layer to mute."
                                    }
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text("Mute top layer") }
                        }

                        /* ---- face ---- */
                        Text("Face", style = MaterialTheme.typography.titleSmall)
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            OutlinedButton(
                                onClick = {
                                    blinkEnabled = !blinkEnabled
                                    session?.setBlinkEnabled(blinkEnabled)
                                    nativeAnimStatus = if (blinkEnabled) {
                                        "Autonomous blinking on (2–5 s interval)."
                                    } else "Autonomous blinking off."
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text(if (blinkEnabled) "Blinking: on" else "Blinking: off") }
                            OutlinedButton(
                                onClick = {
                                    session?.triggerBlink()
                                    nativeAnimStatus = "Blink triggered."
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text("Blink now") }
                            OutlinedButton(
                                onClick = {
                                    session?.resetFace()
                                    nativeAnimStatus = "Face reset to neutral."
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text("Reset face") }
                        }
                        Text(
                            "Expression amount · ${"%.2f".format(expressionAmount)}",
                            style = MaterialTheme.typography.bodySmall
                        )
                        Slider(
                            value = expressionAmount,
                            onValueChange = { expressionAmount = it },
                            valueRange = 0f..1f
                        )
                        FacialExpression.entries.chunked(4).forEach { rowExpressions ->
                            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                rowExpressions.forEach { expression ->
                                    OutlinedButton(
                                        onClick = {
                                            val written = driver.setExpression(expression, expressionAmount)
                                            nativeAnimStatus = when {
                                                written > 0 ->
                                                    "${expression.name} applied to $written morph channel(s)."
                                                written == 0 ->
                                                    "${expression.name} wrote nothing: this asset has no ARKit-named " +
                                                        "morph targets for it."
                                                else -> "The expression was refused: ${session?.lastError() ?: "no session"}"
                                            }
                                        },
                                        modifier = Modifier.weight(1f)
                                    ) { Text(expression.name, style = MaterialTheme.typography.labelSmall) }
                                }
                            }
                        }
                        val unbound = session?.unboundFaceChannels() ?: emptyList()
                        if (unbound.isNotEmpty()) {
                            Text(
                                "Channels requested but absent from this asset: ${unbound.joinToString(", ")}",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }

                        /* ---- lip-sync ---- */
                        Text("Lip-sync (approximate)", style = MaterialTheme.typography.titleSmall)
                        OutlinedTextField(
                            value = lipSyncText,
                            onValueChange = { lipSyncText = it },
                            label = { Text("Utterance") },
                            modifier = Modifier.fillMaxWidth()
                        )
                        Text(
                            "Duration · ${"%.1f".format(lipSyncSeconds)} s",
                            style = MaterialTheme.typography.bodySmall
                        )
                        Slider(
                            value = lipSyncSeconds,
                            onValueChange = { lipSyncSeconds = it },
                            valueRange = 0.5f..12.0f
                        )
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            Button(
                                onClick = {
                                    val keys = driver.speak(lipSyncText, lipSyncSeconds)
                                    nativeAnimStatus = when {
                                        keys > 0 -> "$keys viseme key(s) scheduled over ${"%.1f".format(lipSyncSeconds)} s."
                                        else -> "No viseme keys: ${driver.status}"
                                    }
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text("Speak") }
                            OutlinedButton(
                                onClick = {
                                    driver.stopSpeaking()
                                    nativeAnimStatus = "Lip-sync stopped."
                                },
                                modifier = Modifier.weight(1f)
                            ) { Text("Stop") }
                        }
                        Text(
                            "This maps SPELLING to a 15-viseme mouth set at a speaking rate derived from the " +
                                "utterance. It is not a phoneme recogniser and does not listen to audio, so it will " +
                                "not match a recording. Anything presented as lip-sync from audio would be a claim " +
                                "this build cannot support.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
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

