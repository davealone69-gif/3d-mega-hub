package com.davemega.hub

import android.net.Uri
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.mutableStateListOf
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
    onGenerated: (Uri, String) -> Unit,
    isImporting: Boolean = false,
    importStatus: String = ""
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
    var previewView by remember { mutableStateOf<FilamentPreviewView?>(null) }
    var showDiagnosticDialog by remember { mutableStateOf(false) }
    var diagnosticRunning by remember { mutableStateOf(false) }
    val diagnosticLog = remember { mutableStateListOf<String>() }
    val generating = generationJob?.isActive == true

    fun captureCurrentScreenshot() {
        if (diagnosticRunning) return
        val activePreview = previewView
        if (activePreview == null) {
            diagnosticLog += "SCREENSHOT FAILED: the Filament preview surface is not available."
            return
        }
        diagnosticRunning = true
        diagnosticLog += "Manual screenshot requested for the currently displayed model."
        activePreview.captureDiagnostic(forceSave = true) { result ->
            diagnosticLog += if (result.frameSubmitted) {
                "PASS: a completed Filament frame was captured."
            } else {
                "FAIL: no completed Filament frame was captured."
            }
            diagnosticLog += "PixelCopy: ${result.width}x${result.height}; qualifying pixels: ${result.pixelsAboveThreshold}"
            result.screenshotPath?.let { diagnosticLog += "SCREENSHOT SAVED: $it" }
            result.error?.let { diagnosticLog += "SCREENSHOT ERROR: $it" }
            diagnosticRunning = false
        }
    }

    fun startGeneratedAvatarDiagnostic() {
        if (diagnosticRunning || generating || isImporting) return
        diagnosticRunning = true
        diagnosticLog.clear()
        showDiagnosticDialog = true
        scope.launch {
            val diagnosticPrompt = "Adult woman, athletic build, 175 cm tall, medium dark hair, black t-shirt and jeans"
            try {
                diagnosticLog += "START: AvatarForge generated-avatar diagnostic"
                diagnosticLog += "Seed: 1337"
                diagnosticLog += "Prompt: $diagnosticPrompt"
                diagnosticLog += "Generating with the real native engine on this device..."
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
                check(result.deformedMeshValidated) {
                    "Native post-skinning vertex validation did not pass."
                }
                diagnosticLog += "PASS: native deformed mesh validation"
                diagnosticLog += "Vertices: ${result.vertexCount}; triangles: ${result.triangleCount}; joints: ${result.jointCount}; blendshapes: ${result.blendshapeCount}"
                diagnosticLog += "GLB: ${result.file.absolutePath} (${result.glbBytes} bytes)"
                val generatedUri = Uri.fromFile(result.file)
                onGenerated(generatedUri, result.file.name)
                val activePreview = previewView
                    ?: error("The 3D viewport has not created its Filament preview surface.")
                activePreview.setModel(generatedUri)
                diagnosticLog += "Loading generated GLB into the 3D-Mega-Hub Filament renderer..."
                activePreview.captureDiagnostic { pixelResult ->
                    diagnosticLog += if (pixelResult.frameSubmitted) {
                        "PASS: Filament beginFrame/render/endFrame completed before PixelCopy."
                    } else {
                        "FAIL: no completed Filament frame was confirmed."
                    }
                    diagnosticLog += if (pixelResult.cameraFrustumPassed) {
                        "PASS: ${pixelResult.cameraFrustumDetail}"
                    } else {
                        "FAIL: ${pixelResult.cameraFrustumDetail}"
                    }
                    diagnosticLog += "PixelCopy: ${pixelResult.width}x${pixelResult.height}; qualifying pixels (R>10 OR G>10 OR B>10): ${pixelResult.pixelsAboveThreshold}"
                    pixelResult.screenshotPath?.let { diagnosticLog += "FAILED RENDER SCREENSHOT: $it" }
                    pixelResult.error?.let { diagnosticLog += "ERROR: $it" }
                    if (pixelResult.error == null && pixelResult.frameSubmitted &&
                        pixelResult.cameraFrustumPassed && pixelResult.pixelsAboveThreshold >= 5000
                    ) {
                        diagnosticLog += "DIAGNOSTIC RESULT: PASS. Pixel threshold reached; physical visual inspection is still required."
                    } else {
                        diagnosticLog += "DIAGNOSTIC RESULT: FAIL/PARTIAL. Inspect the log and screenshot before changing renderer code."
                    }
                    diagnosticRunning = false
                }
            } catch (failure: Exception) {
                diagnosticLog += "DIAGNOSTIC FAILED: ${failure.message ?: failure.javaClass.simpleName}"
                diagnosticRunning = false
            }
        }
    }

    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("Avatar studio", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        Text("A real Filament glTF viewer with the AvatarForge generation kernel. The bundled sample loads automatically; import a self-contained GLB or generate a new avatar below.")

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("3D viewport", style = MaterialTheme.typography.titleMedium)
                AndroidView(
                    factory = { viewContext ->
                        FilamentPreviewView(viewContext).apply {
                            previewView = this
                            setStatusListener { rendererStatus = it }
                            setModel(avatarUri)
                        }
                    },
                    update = { view ->
                        previewView = view
                        view.setModel(avatarUri)
                    },
                    modifier = Modifier.fillMaxWidth().height(420.dp)
                )
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
                                val result = AvatarForgeEngine.generate(
                                    context = context,
                                    spec = AvatarSpec(prompt = generationPrompt.trim()),
                                    prompt = generationPrompt.trim()
                                )
                                onGenerated(Uri.fromFile(result.file), result.file.name)
                                val warning = result.promptMatchWarning?.let { " · WARNING: $it" }.orEmpty()
                                generationStatus = "Generated ${result.triangleCount} triangles · ${result.vertexCount} vertices · ${result.jointCount} joints · ${result.blendshapeCount} blendshapes · ${result.glbBytes / 1024L} KiB GLB$warning"
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
                if (BuildConfig.DEBUG) {
                    OutlinedButton(
                        onClick = { startGeneratedAvatarDiagnostic() },
                        enabled = !diagnosticRunning && !generating && !isImporting,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(if (diagnosticRunning) "Running diagnostic…" else "Run generated-avatar diagnostic")
                    }
                }
            }
        }
    }

    if (BuildConfig.DEBUG && showDiagnosticDialog) {
        AlertDialog(
            onDismissRequest = { if (!diagnosticRunning) showDiagnosticDialog = false },
            title = { Text("Generated-avatar diagnostics") },
            text = {
                Column(
                    modifier = Modifier.fillMaxWidth().height(300.dp).verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    diagnosticLog.forEach { line ->
                        Text(line, style = MaterialTheme.typography.bodySmall)
                    }
                    if (diagnosticRunning) CircularProgressIndicator()
                }
            },
            confirmButton = {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    TextButton(
                        onClick = { captureCurrentScreenshot() },
                        enabled = !diagnosticRunning
                    ) { Text("Capture screenshot") }
                    TextButton(
                        onClick = { startGeneratedAvatarDiagnostic() },
                        enabled = !diagnosticRunning && !generating && !isImporting
                    ) { Text("Run again") }
                }
            },
            dismissButton = {
                TextButton(onClick = { showDiagnosticDialog = false }, enabled = !diagnosticRunning) {
                    Text("Close")
                }
            }
        )
    }
}
