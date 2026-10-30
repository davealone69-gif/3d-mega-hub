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
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.util.Locale
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
    var endpoint by remember { mutableStateOf(prefs.getString("ollama_url", "http://127.0.0.1:11434") ?: "http://127.0.0.1:11434") }
    var model by remember { mutableStateOf(prefs.getString("ollama_model", "llama3.2:1b") ?: "llama3.2:1b") }
    var connection by remember { mutableStateOf("Not checked") }
    var busy by remember { mutableStateOf(false) }
    var prompt by remember { mutableStateOf("") }
    var chat by remember { mutableStateOf(listOf<ChatLine>()) }
    var avatarUri by remember { mutableStateOf(prefs.getString("avatar_uri", null)?.let(Uri::parse)) }
    var avatarName by remember { mutableStateOf(prefs.getString("avatar_name", "No avatar imported") ?: "No avatar imported") }
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

    val avatarPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            try {
                context.contentResolver.takePersistableUriPermission(uri, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (_: Exception) { }
            avatarUri = uri
            avatarName = uri.lastPathSegment?.substringAfterLast('/') ?: "Imported avatar file"
            prefs.edit().putString("avatar_uri", uri.toString()).putString("avatar_name", avatarName).apply()
        }
    }

    fun checkOllama() {
        busy = true
        connection = "Checking $endpoint ..."
        thread {
            val result = try {
                val url = URL(endpoint.trimEnd('/') + "/api/tags")
                val conn = (url.openConnection() as HttpURLConnection).apply {
                    connectTimeout = 3500; readTimeout = 3500; requestMethod = "GET"
                }
                try {
                    val body = conn.inputStream.bufferedReader().use { it.readText() }
                    val names = JSONObject(body).optJSONArray("models")
                    val count = names?.length() ?: 0
                    "Connected. Ollama returned $count model(s)."
                } finally { conn.disconnect() }
            } catch (e: Exception) { "Connection failed: ${e.message ?: e.javaClass.simpleName}" }
            (context as? ComponentActivity)?.runOnUiThread { connection = result; busy = false }
        }
    }

    fun sendPrompt() {
        val question = prompt.trim()
        if (question.isEmpty() || busy) return
        chat = chat + ChatLine(true, question)
        prompt = ""
        busy = true
        chat = chat + ChatLine(false, "Thinking…")
        thread {
            val result = try {
                val payload = JSONObject()
                    .put("model", model.trim())
                    .put("stream", false)
                    .put("messages", org.json.JSONArray().put(JSONObject().put("role", "user").put("content", question)))
                val conn = (URL(endpoint.trimEnd('/') + "/api/chat").openConnection() as HttpURLConnection).apply {
                    connectTimeout = 8000; readTimeout = 120000; requestMethod = "POST"
                    doOutput = true
                    setRequestProperty("Content-Type", "application/json")
                }
                try {
                    conn.outputStream.use { it.write(payload.toString().toByteArray(Charsets.UTF_8)) }
                    val stream = if (conn.responseCode in 200..299) conn.inputStream else conn.errorStream
                    val body = stream.bufferedReader().use { it.readText() }
                    if (conn.responseCode !in 200..299) error("HTTP ${conn.responseCode}: $body")
                    JSONObject(body).optJSONObject("message")?.optString("content").orEmpty().ifBlank { "Ollama returned an empty response." }
                } finally { conn.disconnect() }
            } catch (e: Exception) { "Request failed: ${e.message ?: e.javaClass.simpleName}" }
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
                    HubCard("3D / Avatar", avatarName, "Import a GLB or glTF asset to your library.") { page = HubPage.AVATAR }
                    HubCard("Local AI", connection, "Endpoint: $endpoint · Model: $model") { page = HubPage.AI }
                    HubCard("Voice", voiceStatus, "Android speech output") { page = HubPage.VOICE }
                    HubCard("Device", "${Build.MANUFACTURER} ${Build.MODEL}", "Android ${Build.VERSION.RELEASE} · API ${Build.VERSION.SDK_INT}") { page = HubPage.SETTINGS }
                }
                HubPage.AVATAR -> {
                    Text("Avatar library", style = MaterialTheme.typography.headlineSmall)
                    Text("Import a .glb or .gltf file from device storage. Import stores access to the original file; rendering and editing are not yet connected.")
                    Button(onClick = { avatarPicker.launch(arrayOf("model/gltf-binary", "model/gltf+json", "application/octet-stream", "*/*")) }, modifier = Modifier.fillMaxWidth()) { Text("Import 3D model") }
                    HubCard("Selected asset", avatarName, avatarUri?.toString() ?: "No file selected")
                    if (avatarUri != null) {
                        OutlinedButton(onClick = {
                            avatarUri = null; avatarName = "No avatar imported"
                            prefs.edit().remove("avatar_uri").remove("avatar_name").apply()
                        }) { Text("Remove from library") }
                    }
                    HubCard("Rendering status", "NOT CONNECTED", "The current Filament preview is a renderer diagnostic, not a GLB avatar viewer.")
                }
                HubPage.AI -> {
                    Text("Local AI assistant", style = MaterialTheme.typography.headlineSmall)
                    OutlinedTextField(endpoint, { endpoint = it }, label = { Text("Ollama URL") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                    OutlinedTextField(model, { model = it }, label = { Text("Model name") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = {
                            prefs.edit().putString("ollama_url", endpoint.trim()).putString("ollama_model", model.trim()).apply()
                            checkOllama()
                        }, enabled = !busy) { Text("Test connection") }
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
                    Button(onClick = {
                        prefs.edit().putString("ollama_url", endpoint.trim()).putString("ollama_model", model.trim()).apply()
                        sendPrompt()
                    }, enabled = !busy && prompt.isNotBlank(), modifier = Modifier.fillMaxWidth()) { Text(if (busy) "Working…" else "Send to Ollama") }
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
                    HubCard("Graphics", "Filament OpenGL ES", "Vulkan is not required. Visible avatar rendering still needs device verification.")
                    HubCard("AI endpoint", endpoint, "Saved locally on this device.")
                    Button(onClick = {
                        prefs.edit().putString("ollama_url", endpoint.trim()).putString("ollama_model", model.trim()).apply()
                        Toast.makeText(context, "Settings saved", Toast.LENGTH_SHORT).show()
                    }, modifier = Modifier.fillMaxWidth()) { Text("Save settings") }
                }
            }
        }
    }
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
