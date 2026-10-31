package com.davemega.hub

import android.app.Activity
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

class MainActivity : Activity() {
    private var glView: InteractiveGlView? = null
    private var diagnosticsLabel: TextView? = null
    private val diagnosticsHandler = Handler(Looper.getMainLooper())
    private val diagnosticsTicker = object : Runnable {
        override fun run() {
            val renderer = glView?.renderer
            diagnosticsLabel?.text = renderer?.diagnostics ?: "Renderer stopped"
            if (glView != null) diagnosticsHandler.postDelayed(this, 1000L)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        showHome()
    }

    private fun showHome() {
        diagnosticsHandler.removeCallbacks(diagnosticsTicker)
        diagnosticsLabel = null
        glView?.onPause()
        glView = null
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(24), dp(20), dp(20))
        }
        content.addView(TextView(this).apply {
            text = "3D Mega Hub"
            textSize = 28f
            setTypeface(null, android.graphics.Typeface.BOLD)
        })
        content.addView(TextView(this).apply {
            text = "First working graphics milestone"
            textSize = 16f
            setPadding(0, dp(6), 0, dp(18))
        })
        content.addView(Button(this).apply {
            text = "OPEN INTERACTIVE 3D SCENE"
            setOnClickListener { showViewer() }
        })
        content.addView(TextView(this).apply {
            text = "Device snapshot"
            textSize = 20f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setPadding(0, dp(22), 0, dp(10))
        })
        val details = listOf(
            "Device" to Build.MODEL,
            "Manufacturer" to Build.MANUFACTURER,
            "Android" to "${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})",
            "Supported ABIs" to Build.SUPPORTED_ABIS.joinToString(),
            "OpenGL ES requirement" to glEsVersion(),
            "Package" to packageName
        )
        details.forEach { (label, value) ->
            content.addView(TextView(this).apply {
                text = "$label: ${value.ifBlank { "Unknown" }}"
                textSize = 15f
                setPadding(0, dp(7), 0, dp(7))
            })
        }
        content.addView(TextView(this).apply {
            text = "Renderer: NOT STARTED. Open the scene to initialise a real OpenGL ES context."
            textSize = 14f
            setPadding(0, dp(18), 0, 0)
        })
        setContentView(ScrollView(this).apply { addView(content) })
    }

    private fun showViewer() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(12), dp(12), dp(12), dp(12))
        }
        root.addView(TextView(this).apply {
            text = "OpenGL ES interactive test"
            textSize = 21f
            setTypeface(null, android.graphics.Typeface.BOLD)
        })
        root.addView(TextView(this).apply {
            text = "Drag to rotate • Pinch to zoom • Reset to recenter"
            textSize = 14f
            setPadding(0, dp(6), 0, dp(10))
        })
        val scene = InteractiveGlView(this)
        glView = scene
        root.addView(scene, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f
        ))
        val controls = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
        }
        controls.addView(Button(this).apply {
            text = "RESET VIEW"
            setOnClickListener { scene.renderer.resetView() }
        })
        controls.addView(Button(this).apply {
            text = "BACK"
            setOnClickListener { showHome() }
        })
        root.addView(controls)
        diagnosticsLabel = TextView(this).apply {
            text = "Waiting for OpenGL context…"
            textSize = 12f
            setPadding(0, dp(8), 0, 0)
        }
        root.addView(diagnosticsLabel)
        root.addView(TextView(this).apply {
            text = "Backend: OpenGL ES 2.0 • Scene: procedural cube • Drag to rotate, pinch to zoom"
            textSize = 12f
            setPadding(0, dp(4), 0, 0)
        })
        setContentView(root)
        diagnosticsHandler.removeCallbacks(diagnosticsTicker)
        diagnosticsHandler.post(diagnosticsTicker)
    }

    private fun glEsVersion(): String {
        val info = packageManager.systemAvailableFeatures.firstOrNull {
            it.name == null && it.reqGlEsVersion > 0
        } ?: return "Not reported"
        val version = info.reqGlEsVersion
        return "${version shr 16}.${version and 0xffff}"
    }

    override fun onResume() {
        super.onResume()
        glView?.onResume()
    }

    override fun onPause() {
        diagnosticsHandler.removeCallbacks(diagnosticsTicker)
        glView?.onPause()
        super.onPause()
    }

    override fun onDestroy() {
        diagnosticsHandler.removeCallbacks(diagnosticsTicker)
        diagnosticsLabel = null
        glView?.onPause()
        glView = null
        super.onDestroy()
    }

    @Deprecated("Deprecated in Android API 33")
    override fun onBackPressed() {
        if (glView != null) showHome() else super.onBackPressed()
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()
}
