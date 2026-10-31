package com.davemega.hub

import android.content.Context
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.opengl.Matrix
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class InteractiveGlView(context: Context) : GLSurfaceView(context) {
    val renderer = CubeRenderer()
    private var previousX = 0f
    private var previousY = 0f
    private val scaleDetector = ScaleGestureDetector(context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                renderer.zoomBy(detector.scaleFactor)
                return true
            }
        })

    init {
        setEGLContextClientVersion(2)
        setRenderer(renderer)
        renderMode = RENDERMODE_CONTINUOUSLY
        isFocusable = true
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                previousX = event.x
                previousY = event.y
            }
            MotionEvent.ACTION_MOVE -> if (event.pointerCount == 1 && !scaleDetector.isInProgress) {
                val dx = event.x - previousX
                val dy = event.y - previousY
                renderer.rotateBy(dx * 0.55f, dy * 0.55f)
                previousX = event.x
                previousY = event.y
            }
        }
        return true
    }
}

class CubeRenderer : GLSurfaceView.Renderer {
    @Volatile var diagnostics: String = "Waiting for OpenGL context"
        private set
    private var frameCount = 0
    private var lastFpsTime = 0L
    private var measuredFps = 0f
    private var glVendor = "Unknown"
    private var glRenderer = "Unknown"
    private var glVersion = "Unknown"
    @Volatile private var angleX = 22f
    @Volatile private var angleY = -28f
    @Volatile private var cameraDistance = 6f
    private var program = 0
    private var positionHandle = 0
    private var colorHandle = 0
    private var matrixHandle = 0
    private lateinit var vertices: FloatBuffer
    private lateinit var colors: FloatBuffer
    private val projection = FloatArray(16)
    private val view = FloatArray(16)
    private val model = FloatArray(16)
    private val mvp = FloatArray(16)

    fun rotateBy(dx: Float, dy: Float) {
        angleY = (angleY + dx) % 360f
        angleX = (angleX + dy).coerceIn(-89f, 89f)
    }

    fun zoomBy(factor: Float) {
        if (factor > 0f) cameraDistance = (cameraDistance / factor).coerceIn(3f, 12f)
    }

    fun resetView() {
        angleX = 22f
        angleY = -28f
        cameraDistance = 6f
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        glVendor = GLES20.glGetString(GLES20.GL_VENDOR) ?: "Unavailable"
        glRenderer = GLES20.glGetString(GLES20.GL_RENDERER) ?: "Unavailable"
        glVersion = GLES20.glGetString(GLES20.GL_VERSION) ?: "Unavailable"
        GLES20.glClearColor(0.055f, 0.075f, 0.11f, 1f)
        GLES20.glEnable(GLES20.GL_DEPTH_TEST)
        try {
            program = createProgram(VERTEX_SHADER, FRAGMENT_SHADER)
            positionHandle = GLES20.glGetAttribLocation(program, "aPosition")
            colorHandle = GLES20.glGetAttribLocation(program, "aColor")
            matrixHandle = GLES20.glGetUniformLocation(program, "uMvp")
            require(positionHandle >= 0 && colorHandle >= 0 && matrixHandle >= 0) {
                "Required shader attributes/uniforms were not found"
            }
            val data = buildGeometry()
            vertices = floatBuffer(data.first)
            colors = floatBuffer(data.second)
            diagnostics = "RENDERER READY • $glRenderer • $glVersion"
        } catch (failure: Exception) {
            program = 0
            diagnostics = "RENDERER FAILED • ${failure.message ?: failure.javaClass.simpleName}"
        }
        lastFpsTime = System.nanoTime()
        frameCount = 0
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        GLES20.glViewport(0, 0, width, height)
        val ratio = width.toFloat() / height.coerceAtLeast(1)
        Matrix.frustumM(projection, 0, -ratio, ratio, -1f, 1f, 2f, 20f)
    }

    override fun onDrawFrame(gl: GL10?) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT or GLES20.GL_DEPTH_BUFFER_BIT)
        val now = System.nanoTime()
        frameCount++
        val elapsed = now - lastFpsTime
        if (elapsed >= 1_000_000_000L) {
            measuredFps = frameCount * 1_000_000_000f / elapsed
            frameCount = 0
            lastFpsTime = now
            val glError = GLES20.glGetError()
            diagnostics = if (program == 0) {
                "RENDERER FAILED • $glRenderer • $glVersion"
            } else {
                "RENDERER READY • ${"%.1f".format(java.util.Locale.US, measuredFps)} FPS • $glVendor • $glRenderer • $glVersion • GL error: 0x${glError.toString(16)}"
            }
        }
        if (program == 0) return
        Matrix.setLookAtM(view, 0, 0f, 0f, cameraDistance, 0f, 0f, 0f, 0f, 1f, 0f)
        Matrix.setIdentityM(model, 0)
        Matrix.rotateM(model, 0, angleX, 1f, 0f, 0f)
        Matrix.rotateM(model, 0, angleY, 0f, 1f, 0f)
        Matrix.multiplyMM(mvp, 0, view, 0, model, 0)
        Matrix.multiplyMM(mvp, 0, projection, 0, mvp, 0)
        GLES20.glUseProgram(program)
        GLES20.glUniformMatrix4fv(matrixHandle, 1, false, mvp, 0)
        vertices.position(0)
        colors.position(0)
        GLES20.glEnableVertexAttribArray(positionHandle)
        GLES20.glVertexAttribPointer(positionHandle, 3, GLES20.GL_FLOAT, false, 0, vertices)
        GLES20.glEnableVertexAttribArray(colorHandle)
        GLES20.glVertexAttribPointer(colorHandle, 4, GLES20.GL_FLOAT, false, 0, colors)
        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, 36)
        GLES20.glDisableVertexAttribArray(positionHandle)
        GLES20.glDisableVertexAttribArray(colorHandle)
    }

    private fun floatBuffer(values: FloatArray): FloatBuffer =
        ByteBuffer.allocateDirect(values.size * 4).order(ByteOrder.nativeOrder())
            .asFloatBuffer().apply { put(values); position(0) }

    private fun buildGeometry(): Pair<FloatArray, FloatArray> {
        val faces = arrayOf(
            floatArrayOf(-1f,-1f, 1f, 1f,-1f, 1f, 1f,1f,1f, -1f,1f,1f),
            floatArrayOf(1f,-1f,-1f, -1f,-1f,-1f, -1f,1f,-1f, 1f,1f,-1f),
            floatArrayOf(-1f,1f,1f, 1f,1f,1f, 1f,1f,-1f, -1f,1f,-1f),
            floatArrayOf(-1f,-1f,-1f, 1f,-1f,-1f, 1f,-1f,1f, -1f,-1f,1f),
            floatArrayOf(1f,-1f,1f, 1f,-1f,-1f, 1f,1f,-1f, 1f,1f,1f),
            floatArrayOf(-1f,-1f,-1f, -1f,-1f,1f, -1f,1f,1f, -1f,1f,-1f)
        )
        val palette = arrayOf(
            floatArrayOf(0.18f,0.72f,1f,1f), floatArrayOf(0.95f,0.32f,0.28f,1f),
            floatArrayOf(0.25f,0.85f,0.55f,1f), floatArrayOf(1f,0.72f,0.22f,1f),
            floatArrayOf(0.65f,0.42f,1f,1f), floatArrayOf(0.2f,0.82f,0.82f,1f)
        )
        val positions = ArrayList<Float>(108)
        val rgba = ArrayList<Float>(144)
        faces.forEachIndexed { index, face ->
            val order = intArrayOf(0,1,2,0,2,3)
            order.forEach { vertex ->
                for (axis in 0..2) positions.add(face[vertex * 3 + axis])
                palette[index].forEach { rgba.add(it) }
            }
        }
        return positions.toFloatArray() to rgba.toFloatArray()
    }

    private fun createProgram(vertexSource: String, fragmentSource: String): Int {
        val vertex = compileShader(GLES20.GL_VERTEX_SHADER, vertexSource)
        val fragment = compileShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource)
        val linked = GLES20.glCreateProgram()
        GLES20.glAttachShader(linked, vertex)
        GLES20.glAttachShader(linked, fragment)
        GLES20.glLinkProgram(linked)
        val status = IntArray(1)
        GLES20.glGetProgramiv(linked, GLES20.GL_LINK_STATUS, status, 0)
        if (status[0] == 0) {
            val error = GLES20.glGetProgramInfoLog(linked)
            GLES20.glDeleteProgram(linked)
            throw IllegalStateException("OpenGL shader link failed: $error")
        }
        GLES20.glDeleteShader(vertex)
        GLES20.glDeleteShader(fragment)
        return linked
    }

    private fun compileShader(type: Int, source: String): Int {
        val shader = GLES20.glCreateShader(type)
        GLES20.glShaderSource(shader, source)
        GLES20.glCompileShader(shader)
        val status = IntArray(1)
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, status, 0)
        if (status[0] == 0) {
            val error = GLES20.glGetShaderInfoLog(shader)
            GLES20.glDeleteShader(shader)
            throw IllegalStateException("OpenGL shader compile failed: $error")
        }
        return shader
    }

    companion object {
        private const val VERTEX_SHADER = """
            uniform mat4 uMvp;
            attribute vec3 aPosition;
            attribute vec4 aColor;
            varying vec4 vColor;
            void main() {
                gl_Position = uMvp * vec4(aPosition, 1.0);
                vColor = aColor;
            }
        """
        private const val FRAGMENT_SHADER = """
            precision mediump float;
            varying vec4 vColor;
            void main() { gl_FragColor = vColor; }
        """
    }
}
