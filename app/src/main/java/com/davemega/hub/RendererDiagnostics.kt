package com.davemega.hub

import android.opengl.GLES20
import android.util.Log
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

/**
 * Values are nullable until the real renderer reports them. The UI must never
 * manufacture "OK" values when no GL context or renderer frame exists.
 */
data class RendererDiagnosticSnapshot(
    val rendererConnected: Boolean = false,
    val shaderLinked: Boolean? = null,
    val textureLoaded: Boolean? = null,
    val drawCallCount: Int? = null,
    val glErrors: List<Int> = emptyList(),
    val frameSequence: Long? = null
)

object RendererDiagnostics {
    private const val TAG = "RendererDiagnostics"
    private const val MAX_ERRORS_PER_CHECK = 32

    /**
     * Call only on the thread that owns the current GLES context, after the
     * operation being diagnosed. Drains the GL error queue to avoid stale
     * errors being misattributed to later operations.
     */
    fun drainGlErrors(operation: String): List<Int> {
        val errors = mutableListOf<Int>()
        repeat(MAX_ERRORS_PER_CHECK) {
            val error = GLES20.glGetError()
            if (error == GLES20.GL_NO_ERROR) return errors
            errors += error
            Log.e(TAG, "$operation: ${errorName(error)} (0x${error.toString(16)})")
        }
        Log.e(TAG, "$operation: stopped draining after $MAX_ERRORS_PER_CHECK errors")
        return errors
    }

    fun errorName(error: Int): String = when (error) {
        GLES20.GL_INVALID_ENUM -> "GL_INVALID_ENUM"
        GLES20.GL_INVALID_VALUE -> "GL_INVALID_VALUE"
        GLES20.GL_INVALID_OPERATION -> "GL_INVALID_OPERATION"
        GLES20.GL_OUT_OF_MEMORY -> "GL_OUT_OF_MEMORY"
        GLES20.GL_INVALID_FRAMEBUFFER_OPERATION -> "GL_INVALID_FRAMEBUFFER_OPERATION"
        else -> "GL_UNKNOWN_ERROR"
    }

    fun value(value: Boolean?): String = when (value) {
        true -> "OK"
        false -> "FAILED"
        null -> "NOT RUN"
    }

    fun format(snapshot: RendererDiagnosticSnapshot): String = buildString {
        appendLine("Renderer: ${if (snapshot.rendererConnected) "CONNECTED" else "NOT CONNECTED"}")
        appendLine("Shader link: ${value(snapshot.shaderLinked)}")
        appendLine("Texture upload: ${value(snapshot.textureLoaded)}")
        appendLine("Draw calls: ${snapshot.drawCallCount?.toString() ?: "NOT AVAILABLE"}")
        appendLine("GL errors: ${if (snapshot.glErrors.isEmpty()) {
            if (snapshot.rendererConnected) "NONE REPORTED" else "NOT AVAILABLE"
        } else snapshot.glErrors.joinToString { "${errorName(it)} (0x${it.toString(16)})" }}")
        append("Frame: ${snapshot.frameSequence?.toString() ?: "NOT AVAILABLE"}")
    }
}

@Composable
fun RendererDiagnosticOverlay(snapshot: RendererDiagnosticSnapshot) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(8.dp)
    ) {
        Column(
            modifier = Modifier
                .background(Color.Black.copy(alpha = 0.82f))
                .padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(3.dp)
        ) {
            Text(
                "RENDERER DIAGNOSTICS",
                color = Color.White,
                style = MaterialTheme.typography.labelLarge,
                fontWeight = FontWeight.Bold
            )
            Text(
                RendererDiagnostics.format(snapshot),
                color = Color(0xFFFFD180),
                style = MaterialTheme.typography.bodySmall
            )
            Text(
                "GL checks must be published by the renderer's GL thread. No renderer is connected in this build.",
                color = Color.LightGray,
                style = MaterialTheme.typography.labelSmall
            )
        }
    }
}
