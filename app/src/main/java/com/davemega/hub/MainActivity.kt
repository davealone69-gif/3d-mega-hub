package com.davemega.hub

import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.speech.tts.TextToSpeech
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.davemega.hub.avatarforge.GlbValidator
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.net.URL
import java.util.Locale
import java.util.UUID
import kotlin.concurrent.thread

private enum class HubPage(val label: String) {
    HOME("Home"), AVATAR("Avatar"), AI("AI"), VOICE("Voice"), SETTINGS("Settings")
}

private data class ChatLine(val fromUser: Boolean, val text: String)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme()) {
                Surface(Modifier.fillMaxSize()) { MegaHubApp() }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MegaHubApp() {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences("mega_hub", Context.MODE_PRIVATE) }
    var page by remember { mutableStateOf(HubPage.HOME) }
    var endpoint by remember { mutableStateOf(prefs.getString("ollama_url", "http://localhost:11434") ?: "http://localhost:11434") }
    var model by remember { mutableStateOf(prefs.getString("ollama_model", "llama3.2:1b") ?: "llama3.2:1b") }
    var connection by remember { mutableStateOf("Not checked") }
    var busy by remember { mutableStateOf(false) }
    var prompt by remember { mutableStateOf("") }
    var chat by remember { mutableStateOf(listOf<ChatLine>()) }
    var avatarUri by remember { mutableStateOf(prefs.getString("avatar_uri", null)?.let(Uri::parse)) }
    var avatarName by remember { mutableStateOf(prefs.getString("avatar_name", "Built-in sample model") ?: "Built-in sample model") }
    var avatarImporting by remember { mutableStateOf(false) }
    var avatarImportStatus by remember { mutableStateOf("Imported GLB files are copied into app-private storage and validated before use.") }
    var voiceText by remember { mutableStateOf("Hello. Your 3D Mega Hub voice output is ready.") }
    var voiceStatus by remember { mutableStateOf("Android text-to-speech is available on this device.") }
    var tts by remember { mutableStateOf<TextToSpeech?>(null) }
    var ttsReady by remember { mutableStateOf(false) }

    DisposableEffect(context) {
        val engine = TextToSpeech(context) { result ->
            ttsReady = result == TextToSpeech.SUCCESS
            voiceStatus = if (ttsReady) "Speech engine ready" else "Speech engine failed to initialise"
        }
        tts = engine
        onDispose { engine.stop(); engine.shutdown(); tts = null }
    }

    fun importAvatar(uri: Uri) {
        if (avatarImporting) return
        val activity = context as? ComponentActivity ?: return
        avatarImporting = true
        avatarImportStatus = "Copying and validating selected GLB…"
        thread(name = "MegaHubAvatarImport") {
            var temporaryFile: File? = null
            try {
                val directory = File(context.filesDir, "avatars")
                check(directory.isDirectory || directory.mkdirs()) { "Could not create the private avatar library directory." }
                val outputFile = File.createTempFile("imported-avatar-", ".glb.part", directory)
                temporaryFile = outputFile
                val input = context.contentResolver.openInputStream(uri)
                    ?: error("Android could not open the selected document.")
                input.use { source ->
                    outputFile.outputStream().use { target ->
                        val buffer = ByteArray(64 * 1024)
                        var total = 0L
                        while (true) {
                            val count = source.read(buffer)
                            if (count < 0) break
                            total += count
                            require(total <= GlbValidator.MAX_GLB_BYTES) {
                                "GLB exceeds the ${GlbValidator.MAX_GLB_BYTES / (1024L * 1024L)} MiB import limit."
                            }
                            target.write(buffer, 0, count)
                        }
                    }
                }
                GlbValidator.validate(outputFile)
                val fileName = "imported-${System.currentTimeMillis()}-${UUID.randomUUID().toString().take(8)}.glb"
                val destination = File(directory, fileName)
                Files.move(outputFile.toPath(), destination.toPath(), StandardCopyOption.ATOMIC_MOVE)
                temporaryFile = null
                val importedUri = Uri.fromFile(destination)
                prefs.edit().putString("avatar_uri", importedUri.toString())
                    .putString("avatar_name", fileName).apply()
                if (!activity.isFinishing && !activity.isDestroyed) {
                    activity.runOnUiThread {
                        avatarUri = importedUri
                        avatarName = fileName
                        avatarImporting = false
                        avatarImportStatus = "Imported and structurally validated · ${destination.length() / 1024L} KiB"
                    }
                }
            } catch (failure: Exception) {
                temporaryFile?.delete()
                if (!activity.isFinishing && !activity.isDestroyed) {
                    activity.runOnUiThread {
                        avatarImporting = false
                        avatarImportStatus = "Import failed: ${failure.message ?: failure.javaClass.simpleName}"
                        Toast.makeText(context, avatarImportStatus, Toast.LENGTH_LONG).show()
                    }
                }
            }
        }
    }

    val avatarPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) importAvatar(uri)
    }

    fun checkOllama() {
        val baseUrl = try {
            OllamaEndpoint.normalize(endpoint)
        } catch (error: IllegalArgumentException) {
            connection = error.message ?: "Invalid Ollama endpoint."
            return
        }
        endpoint = baseUrl
        prefs.edit().putString("ollama_url", baseUrl).apply()
        busy = true
        connection = "Checking $baseUrl ..."
        thread(name = "MegaHubOllamaCheck") {
            val result = try {
                val conn = (URL("$baseUrl/api/tags").openConnection() as HttpURLConnection).apply {
                    connectTimeout = 4_000
                    readTimeout = 5_000
                    requestMethod = "GET"
                    instanceFollowRedirects = false
                    useCaches = false
                    setRequestProperty("Accept", "application/json")
                }
                try {
                    val code = conn.responseCode
                    val stream = if (code in 200..299) conn.inputStream else conn.errorStream
                    val body = stream?.use { readLimitedResponse(it, 1_048_576) }.orEmpty()
                    if (code !in 200..299) error("Ollama returned HTTP $code: ${body.take(1_024)}")
                    val names = JSONObject(body).optJSONArray("models")
                    "Connected. Ollama returned ${names?.length() ?: 0} model(s)."
                } finally {
                    conn.disconnect()
                }
            } catch (error: Exception) {
                "Connection failed: ${error.message ?: error.javaClass.simpleName}"
            }
            (context as? ComponentActivity)?.runOnUiThread {
                connection = result
                busy = false
            }
        }
    }

    fun sendPrompt() {
        if (busy) return
        val question = prompt.trim()
        if (question.isEmpty()) return
        if (question.length > 32_768) {
            connection = "Message is too long. Keep it under 32,768 characters."
            return
        }
        val baseUrl = try {
            OllamaEndpoint.normalize(endpoint)
        } catch (error: IllegalArgumentException) {
            connection = error.message ?: "Invalid Ollama endpoint."
            return
        }
        val modelName = try {
            OllamaEndpoint.validateModel(model)
        } catch (error: IllegalArgumentException) {
            connection = error.message ?: "Invalid model name."
            return
        }
        endpoint = baseUrl
        model = modelName
        prefs.edit().putString("ollama_url", baseUrl).putString("ollama_model", modelName).apply()
        chat = chat + ChatLine(true, question)
        prompt = ""
        busy = true
        chat = chat + ChatLine(false, "Thinking…")
        thread(name = "MegaHubOllamaChat") {
            val result = try {
                val payload = JSONObject()
                    .put("model", modelName)
                    .put("stream", false)
                    .put("messages", org.json.JSONArray().put(JSONObject().put("role", "user").put("content", question)))
                val conn = (URL("$baseUrl/api/chat").openConnection() as HttpURLConnection).apply {
                    connectTimeout = 8_000
                    readTimeout = 120_000
                    requestMethod = "POST"
                    instanceFollowRedirects = false
                    useCaches = false
                    doOutput = true
                    setRequestProperty("Content-Type", "application/json; charset=utf-8")
                    setRequestProperty("Accept", "application/json")
                }
                try {
                    conn.outputStream.use { it.write(payload.toString().toByteArray(Charsets.UTF_8)) }
                    val code = conn.responseCode
                    val stream = if (code in 200..299) conn.inputStream else conn.errorStream
                    val body = stream?.use { readLimitedResponse(it, 4_194_304) }.orEmpty()
                    if (code !in 200..299) error("Ollama returned HTTP $code: ${body.take(2_048)}")
                    JSONObject(body).optJSONObject("message")?.optString("content").orEmpty()
                        .ifBlank { "Ollama returned an empty response." }
                } finally {
                    conn.disconnect()
                }
            } catch (error: Exception) {
                "Request failed: ${error.message ?: error.javaClass.simpleName}"
            }
            (context as? ComponentActivity)?.runOnUiThread {
                chat = chat.dropLast(1) + ChatLine(false, result)
                busy = false
            }
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(title = { Text("3D Mega Hub", fontWeight = FontWeight.Bold) })
        },
        bottomBar = {
            NavigationBar {
                HubPage.entries.forEach { item ->
                    NavigationBarItem(
                        selected = page == item,
                        onClick = { page = item },
                        icon = { Text(when (item) { HubPage.HOME -> "⌂"; HubPage.AVATAR -> "◇"; HubPage.AI -> "✦"; HubPage.VOICE -> "♫"; HubPage.SETTINGS -> "⚙" }) },
                        label = { Text(item.label) }
                    )
                }
            }
        }
    ) { padding ->
        Column(
            Modifier.fillMaxSize().padding(padding).imePadding().verticalScroll(rememberScrollState()).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            when (page) {
                HubPage.HOME -> {
                    Text("Your creative workspace", style = MaterialTheme.typography.headlineSmall)
                    Text("A single hub for 3D assets, local AI and voice. Each integration reports its real connection state.")
                    HubCard("3D / Avatar", avatarName, "Import a GLB or generate an avatar on-device.") { page = HubPage.AVATAR }
                    HubCard("Local AI", connection, "Endpoint: $endpoint · Model: $model") { page = HubPage.AI }
                    HubCard("Voice", voiceStatus, "Android speech output") { page = HubPage.VOICE }
                    HubCard("Device", "${Build.MANUFACTURER} ${Build.MODEL}", "Android ${Build.VERSION.RELEASE} · API ${Build.VERSION.SDK_INT}") { page = HubPage.SETTINGS }
                }
                HubPage.AVATAR -> {
                    AvatarStudioScreen(
                        avatarUri = avatarUri,
                        avatarName = avatarName,
                        onImport = {
                            avatarPicker.launch(arrayOf("model/gltf-binary", "application/octet-stream"))
                        },
                        onRemove = {
                            avatarUri = null
                            avatarName = "Built-in sample model"
                            prefs.edit().remove("avatar_uri").remove("avatar_name").apply()
                        },
                        onGenerated = { uri, name ->
                            avatarUri = uri
                            avatarName = name
                            prefs.edit().putString("avatar_uri", uri.toString())
                                .putString("avatar_name", name).apply()
                        },
                        isImporting = avatarImporting,
                        importStatus = avatarImportStatus
                    )
                }
                HubPage.AI -> {
                    Text("Local AI assistant", style = MaterialTheme.typography.headlineSmall)
                    OutlinedTextField(endpoint, { endpoint = it }, label = { Text("Ollama URL") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                    OutlinedTextField(model, { model = it }, label = { Text("Model name") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = { checkOllama() }, enabled = !busy) { Text("Test connection") }
                        Text(connection, modifier = Modifier.weight(1f), style = MaterialTheme.typography.bodySmall)
                    }
                    Text("Chat", style = MaterialTheme.typography.titleLarge)
                    chat.forEach { line ->
                        Card(Modifier.fillMaxWidth()) {
                            Column(Modifier.padding(12.dp)) {
                                Text(if (line.fromUser) "YOU" else "ASSISTANT", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.primary)
                                Text(line.text)
                            }
                        }
                    }
                    OutlinedTextField(prompt, { prompt = it }, label = { Text("Message your local model") }, modifier = Modifier.fillMaxWidth(), minLines = 2)
                    Button(onClick = { sendPrompt() }, enabled = !busy && prompt.isNotBlank(), modifier = Modifier.fillMaxWidth()) { Text(if (busy) "Working…" else "Send to Ollama") }
                    Text("This sends requests directly to the configured Ollama /api/chat endpoint. It does not use a cloud AI service.", style = MaterialTheme.typography.bodySmall)
                }
                HubPage.VOICE -> {
                    Text("Voice output", style = MaterialTheme.typography.headlineSmall)
                    Text(voiceStatus)
                    OutlinedTextField(voiceText, { voiceText = it }, label = { Text("Text to speak") }, modifier = Modifier.fillMaxWidth(), minLines = 3)
                    Button(onClick = {
                        if (ttsReady) tts?.speak(voiceText, TextToSpeech.QUEUE_FLUSH, null, "mega-hub-voice")
                        else Toast.makeText(context, "Speech engine is not ready", Toast.LENGTH_SHORT).show()
                    }, enabled = ttsReady, modifier = Modifier.fillMaxWidth()) { Text("Speak") }
                    OutlinedButton(onClick = { tts?.stop() }, modifier = Modifier.fillMaxWidth()) { Text("Stop speech") }
                    Text("Speech uses the Android voices installed on this phone. Viseme extraction and avatar lip-sync are not connected yet.")
                }
                HubPage.SETTINGS -> {
                    Text("System & settings", style = MaterialTheme.typography.headlineSmall)
                    HubCard("Runtime", "Android ${Build.VERSION.RELEASE}", "API ${Build.VERSION.SDK_INT} · ${Build.SUPPORTED_ABIS.joinToString()}")
                    HubCard("Graphics", "Filament OpenGL ES (Vulkan fallback)", "OpenGL ES is the primary path. Vulkan is attempted only if GLES initialization fails; device rendering remains unverified.")
                    HubCard("AI endpoint", endpoint, "Saved locally on this device.")
                    Button(onClick = {
                        try {
                            val normalized = OllamaEndpoint.normalize(endpoint)
                            val validModel = OllamaEndpoint.validateModel(model)
                            endpoint = normalized
                            model = validModel
                            prefs.edit().putString("ollama_url", normalized).putString("ollama_model", validModel).apply()
                            Toast.makeText(context, "Settings saved", Toast.LENGTH_SHORT).show()
                        } catch (error: IllegalArgumentException) {
                            Toast.makeText(context, error.message ?: "Invalid settings", Toast.LENGTH_LONG).show()
                        }
                    }, modifier = Modifier.fillMaxWidth()) { Text("Save settings") }
                }
            }
        }
    }
}


private fun readLimitedResponse(input: java.io.InputStream, maxBytes: Int): String {
    val output = java.io.ByteArrayOutputStream()
    val buffer = ByteArray(8_192)
    var total = 0
    while (true) {
        val count = input.read(buffer)
        if (count < 0) break
        total += count
        require(total <= maxBytes) { "Ollama response exceeded the $maxBytes-byte safety limit." }
        output.write(buffer, 0, count)
    }
    return output.toString(Charsets.UTF_8.name())
}

@Composable
private fun HubCard(title: String, status: String, detail: String, onClick: (() -> Unit)? = null) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            Text(status, style = MaterialTheme.typography.bodyLarge)
            Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            if (onClick != null) TextButton(onClick = onClick) { Text("Open") }
        }
    }
}
