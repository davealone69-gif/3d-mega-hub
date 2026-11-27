package com.davemega.hub.avatarforge

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.PixelCopy
import android.view.ScaleGestureDetector
import android.view.SurfaceHolder
import android.view.SurfaceView
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.math.abs
import kotlin.math.max

/** One render worker owns the AvatarForge engine, renderer and ANativeWindow lifetime. */
class AvatarForgeSurfaceView(
    context: Context,
    private val glbFile: File,
    private val verifyVisiblePixels: Boolean = false,
    private val preferVulkan: Boolean = true,
    private val allowLiveRig: Boolean = true,
    private val onCapabilities: (jointCount: Int, blendshapeCount: Int, isLiveRig: Boolean) -> Unit = { _, _, _ -> },
    private val onRegionPicked: (regionId: Int, distanceM: Float) -> Unit = { _, _ -> },
    private val onStatus: (String) -> Unit
) : SurfaceView(context), SurfaceHolder.Callback {
    private val running = AtomicBoolean(false)
    private val startPending = AtomicBoolean(false)
    private val pixelCopyPending = AtomicBoolean(false)
    private val pixelCopyComplete = AtomicBoolean(false)
    private val pixelCopyAttempts = AtomicInteger(0)
    private val pixelCopyActiveAttempt = AtomicInteger(0)
    private val mainHandler = Handler(Looper.getMainLooper())
    @Volatile private var surfaceReady = false
    @Volatile private var surfaceWidth = 0
    @Volatile private var surfaceHeight = 0
    @Volatile private var renderThread: Thread? = null

    private val cameraLock = Any()
    private var pendingYawDeg = 0f
    private var pendingPitchDeg = 0f
    private var pendingZoomScale = 1f
    private var pendingCameraReset = false
    private var pendingCameraPresetId: Int? = null
    private var pendingLightingPresetId: Int? = null
    private var pendingInspectionDirty = false
    private var showSkeleton = false
    private var flatShading = false
    private var pendingClipId: Int? = null
    private var pendingClipSpeed: Float = 1.0f
    private var pendingClipLoop: Boolean = true
    private var pendingExpressionId: Int? = null
    private var pendingPickNormX: Float? = null
    private var pendingPickNormY: Float? = null
    private var pendingClearHighlight = false
    @Volatile private var autoTurntable = false
    @Volatile private var userInteracting = false
    private var lastTouchX = 0f
    private var lastTouchY = 0f
    private var activePointerId = MotionEvent.INVALID_POINTER_ID

    fun setCameraPreset(preset: CameraPreset) {
        synchronized(cameraLock) {
            pendingYawDeg = 0f
            pendingPitchDeg = 0f
            pendingZoomScale = 1f
            pendingCameraReset = false
            pendingCameraPresetId = preset.nativeId
        }
    }

    fun setLightingPreset(preset: LightingPreset) {
        synchronized(cameraLock) {
            pendingLightingPresetId = preset.nativeId
        }
    }

    fun setSkeletonVisible(visible: Boolean) {
        synchronized(cameraLock) {
            if (showSkeleton != visible) {
                showSkeleton = visible
                pendingInspectionDirty = true
            }
        }
    }

    fun setFlatShading(enabled: Boolean) {
        synchronized(cameraLock) {
            if (flatShading != enabled) {
                flatShading = enabled
                pendingInspectionDirty = true
            }
        }
    }

    fun setAutoTurntable(enabled: Boolean) {
        autoTurntable = enabled
    }

    fun setAnimationClip(clip: AnimationClipPreset, speed: Float = 1.0f, loop: Boolean = true) {
        synchronized(cameraLock) {
            pendingClipId = clip.nativeId
            pendingClipSpeed = speed.coerceIn(0.1f, 3.0f)
            pendingClipLoop = loop
        }
    }

    fun setFacialExpression(expression: FacialExpressionPreset) {
        synchronized(cameraLock) {
            pendingExpressionId = expression.nativeId
        }
    }

    fun clearRegionHighlight() {
        synchronized(cameraLock) {
            pendingPickNormX = null
            pendingPickNormY = null
            pendingClearHighlight = true
        }
    }

    private val scaleDetector = ScaleGestureDetector(
        context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                val factor = detector.scaleFactor
                if (factor.isFinite() && factor > 0f) {
                    synchronized(cameraLock) {
                        pendingZoomScale = (pendingZoomScale / factor).coerceIn(0.25f, 4.0f)
                    }
                }
                return true
            }
        }
    )

    private val tapDetector = GestureDetector(
        context,
        object : GestureDetector.SimpleOnGestureListener() {
            override fun onSingleTapConfirmed(e: MotionEvent): Boolean {
                val w = width
                val h = height
                if (w > 0 && h > 0) {
                    val nx = (e.x / w.toFloat()).coerceIn(0f, 1f)
                    val ny = (e.y / h.toFloat()).coerceIn(0f, 1f)
                    synchronized(cameraLock) {
                        pendingPickNormX = nx
                        pendingPickNormY = ny
                        pendingClearHighlight = false
                    }
                }
                return true
            }

            override fun onDoubleTap(e: MotionEvent): Boolean {
                synchronized(cameraLock) {
                    pendingYawDeg = 0f
                    pendingPitchDeg = 0f
                    pendingZoomScale = 1f
                    pendingCameraReset = true
                }
                return true
            }
        }
    )

    init {
        holder.setFormat(PixelFormat.OPAQUE)
        holder.addCallback(this)
        isFocusable = true
        isClickable = true
        // Do NOT set a non-transparent View background on SurfaceView: the View layer sits above
        // the default Z = -2 EGL surface queue and would paint over the window punch-hole.
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            parent?.requestDisallowInterceptTouchEvent(true)
        }
        tapDetector.onTouchEvent(event)
        scaleDetector.onTouchEvent(event)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                userInteracting = true
                activePointerId = event.getPointerId(0)
                lastTouchX = event.x
                lastTouchY = event.y
            }
            MotionEvent.ACTION_MOVE -> {
                if (!scaleDetector.isInProgress && event.pointerCount == 1) {
                    val idx = event.findPointerIndex(activePointerId)
                    if (idx >= 0) {
                        val x = event.getX(idx)
                        val y = event.getY(idx)
                        val dx = x - lastTouchX
                        val dy = y - lastTouchY
                        lastTouchX = x
                        lastTouchY = y
                        val viewSpan = max(1, minOf(width, height)).toFloat()
                        synchronized(cameraLock) {
                            pendingYawDeg -= (dx / viewSpan) * 180.0f
                            pendingPitchDeg += (dy / viewSpan) * 120.0f
                        }
                    }
                }
            }
            MotionEvent.ACTION_POINTER_UP -> {
                val pointerIndex = event.actionIndex
                if (event.getPointerId(pointerIndex) == activePointerId) {
                    val newIndex = if (pointerIndex == 0) 1 else 0
                    if (newIndex < event.pointerCount) {
                        activePointerId = event.getPointerId(newIndex)
                        lastTouchX = event.getX(newIndex)
                        lastTouchY = event.getY(newIndex)
                    }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                userInteracting = false
                activePointerId = MotionEvent.INVALID_POINTER_ID
                parent?.requestDisallowInterceptTouchEvent(false)
            }
        }
        return true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceCreated valid=${holder.surface.isValid}")
        stopRenderer()
        // A Surface callback runs on the UI thread. Stop the old worker without joining it:
        // native renderer teardown may wait for GPU work and must never block input dispatch.
        surfaceReady = true
        pixelCopyPending.set(false)
        pixelCopyComplete.set(false)
        // Keep the attempt counter monotonic across Surface recreation so a late
        // callback from the previous Surface cannot match a new request token.
        pixelCopyActiveAttempt.set(0)
        startRenderer(holder)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "surfaceChanged format=$format size=${width}x${height}")
        surfaceWidth = width
        surfaceHeight = height
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceDestroyed")
        surfaceReady = false
        pixelCopyActiveAttempt.set(0)
        pixelCopyPending.set(false)
        pixelCopyComplete.set(true)
        stopRenderer()
    }

    private fun stopRenderer() {
        // surfaceDestroyed/surfaceCreated are dispatched on the main thread. The render
        // worker owns EGL, ANativeWindow and engine teardown, so only signal it here.
        // startRenderer() serializes any replacement session behind worker completion.
        running.set(false)
    }

    private fun surfaceHolderIsUsable(holder: SurfaceHolder): Boolean =
        holder.surface.isValid && surfaceReady

    private fun startRenderer(surfaceHolder: SurfaceHolder) {
        val previous = renderThread
        if (previous != null && previous.isAlive) {
            // Never join a render worker from the UI thread. Renderer destruction can wait
            // for outstanding GPU commands; a five-second join here is enough to trigger
            // Android's input-dispatch ANR during Activity/Surface recreation.
            if (startPending.compareAndSet(false, true)) {
                Thread({
                    try {
                        previous.join()
                    } catch (interrupted: InterruptedException) {
                        Thread.currentThread().interrupt()
                    } finally {
                        mainHandler.post {
                            startPending.set(false)
                            if (surfaceReady && surfaceHolderIsUsable(surfaceHolder)) {
                                startRenderer(surfaceHolder)
                            }
                        }
                    }
                }, "AvatarForge-GLES-Restart").apply {
                    isDaemon = true
                    start()
                }
            }
            return
        }
        if (renderThread === previous) renderThread = null
        if (!running.compareAndSet(false, true)) return
        val worker = Thread({
            var nativeHandle = 0L
            var failureStatus: String? = null
            try {
                recordSmokeResult("STARTING")
                onStatus("Starting AvatarForge renderer and importing ${glbFile.name}…")
                while (running.get() && surfaceReady && (surfaceWidth <= 0 || surfaceHeight <= 0)) Thread.sleep(10)
                if (!running.get() || !surfaceReady) return@Thread
                check(glbFile.isFile && glbFile.length() > 0L) { "Selected GLB is not available in app storage." }
                Log.i(TAG, "nativeCreate GLB=${glbFile.absolutePath} size=${surfaceWidth}x${surfaceHeight} preferVulkan=$preferVulkan allowLiveRig=$allowLiveRig")
                nativeHandle = nativeCreate(
                    surfaceHolder.surface,
                    glbFile.absolutePath,
                    max(1, surfaceWidth),
                    max(1, surfaceHeight),
                    preferVulkan,
                    allowLiveRig
                )
                check(nativeHandle != 0L) { "AvatarForge native renderer returned a null session." }
                val caps = nativeGetCapabilities(nativeHandle)
                if (caps.size == 3) {
                    val joints = caps[0]
                    val shapes = caps[1]
                    val liveRig = caps[2] == 1
                    mainHandler.post { onCapabilities(joints, shapes, liveRig) }
                }
                val rendererInfo = nativeGetInfo(nativeHandle)
                val descriptor = RendererBackendPolicy.parse(rendererInfo)
                val backendLabel = descriptor.backend.displayName
                val apiVersion = descriptor.apiVersion
                val firstPresentBudgetNanos = RendererFrameBudget.firstPresentBudgetNanos(rendererInfo)
                val firstPresentBudgetSeconds = firstPresentBudgetNanos / 1_000_000_000L
                recordSmokeResult("RENDERER_ACTIVE", apiVersion = apiVersion, backend = descriptor.backend.id)
                Log.i(TAG, "RENDERER_ACTIVE backend=${descriptor.backend.id} device=${descriptor.deviceSummary} api=${descriptor.apiVersion}")
                onStatus("AvatarForge $backendLabel renderer active · ${descriptor.deviceSummary} · ${glbFile.name}")
                var lastFrame = System.nanoTime()
                var lastWidth = surfaceWidth
                var lastHeight = surfaceHeight
                var reportedFrame = false
                val frameWaitStarted = System.nanoTime()
                while (running.get() && surfaceReady) {
                    val width = surfaceWidth
                    val height = surfaceHeight
                    if (width > 0 && height > 0 && (width != lastWidth || height != lastHeight)) {
                        nativeResize(nativeHandle, width, height)
                        lastWidth = width
                        lastHeight = height
                    }
                    var deltaYaw = 0f
                    var deltaPitch = 0f
                    var zoomScale = 1f
                    var resetCam = false
                    var cameraPresetId: Int? = null
                    var lightingPresetId: Int? = null
                    var applyInspection = false
                    var skeletonNow = false
                    var flatNow = false
                    var clipIdNow: Int? = null
                    var clipSpeedNow = 1.0f
                    var clipLoopNow = true
                    var exprIdNow: Int? = null
                    var pickXNow: Float? = null
                    var pickYNow: Float? = null
                    var clearHighlightNow = false
                    synchronized(cameraLock) {
                        deltaYaw = pendingYawDeg
                        deltaPitch = pendingPitchDeg
                        zoomScale = pendingZoomScale
                        resetCam = pendingCameraReset
                        cameraPresetId = pendingCameraPresetId
                        lightingPresetId = pendingLightingPresetId
                        applyInspection = pendingInspectionDirty
                        skeletonNow = showSkeleton
                        flatNow = flatShading
                        clipIdNow = pendingClipId
                        clipSpeedNow = pendingClipSpeed
                        clipLoopNow = pendingClipLoop
                        exprIdNow = pendingExpressionId
                        pickXNow = pendingPickNormX
                        pickYNow = pendingPickNormY
                        clearHighlightNow = pendingClearHighlight
                        pendingYawDeg = 0f
                        pendingPitchDeg = 0f
                        pendingZoomScale = 1f
                        pendingCameraReset = false
                        pendingCameraPresetId = null
                        pendingLightingPresetId = null
                        pendingInspectionDirty = false
                        pendingClipId = null
                        pendingExpressionId = null
                        pendingPickNormX = null
                        pendingPickNormY = null
                        pendingClearHighlight = false
                    }
                    val now = System.nanoTime()
                    val delta = ((now - lastFrame).toDouble() / 1_000_000_000.0).coerceIn(0.001, 0.1).toFloat()
                    lastFrame = now
                    if (lightingPresetId != null) {
                        nativeSetLightingPreset(nativeHandle, lightingPresetId!!)
                    }
                    if (applyInspection) {
                        nativeSetInspectionModes(nativeHandle, skeletonNow, flatNow)
                    }
                    if (clipIdNow != null) {
                        nativeSetAnimationClip(nativeHandle, clipIdNow!!, clipSpeedNow, clipLoopNow)
                    }
                    if (exprIdNow != null) {
                        nativeApplyExpression(nativeHandle, exprIdNow!!)
                    }
                    if (cameraPresetId != null) {
                        nativeSetCameraPreset(nativeHandle, cameraPresetId!!)
                    } else if (resetCam) {
                        nativeResetCamera(nativeHandle)
                    } else {
                        if (autoTurntable && !userInteracting && reportedFrame) {
                            deltaYaw += delta * 24.0f
                        }
                        if (deltaYaw != 0f || deltaPitch != 0f || zoomScale != 1f) {
                            nativeOrbitCamera(nativeHandle, deltaYaw, deltaPitch, zoomScale)
                        }
                    }
                    if (clearHighlightNow) {
                        nativeClearHighlight(nativeHandle)
                        mainHandler.post { onRegionPicked(-1, 0f) }
                    } else if (pickXNow != null && pickYNow != null) {
                        val picked = nativePickRegion(nativeHandle, pickXNow!!, pickYNow!!)
                        if (picked.size == 2) {
                            val hitRegion = picked[0].toInt()
                            val hitDist = picked[1]
                            mainHandler.post { onRegionPicked(hitRegion, hitDist) }
                        }
                    }
                    val presentedThisFrame = nativeRender(nativeHandle, delta)
                    val stats = nativeGetStats(nativeHandle)
                    check(stats.size == 9) { "AvatarForge returned ${stats.size} renderer statistics; expected 9." }
                    val frames = stats[0]
                    val drawCalls = stats[1]
                    val triangles = stats[2]
                    val framePresented = stats[3] == 1
                    val presentedFrames = stats[4]
                    val boundsInFrustum = stats[5] == 1
                    val outsideCorners = stats[6]
                    val glError = stats[7]
                    val eglError = stats[8]
                    if (!reportedFrame) {
                        if (presentedThisFrame && framePresented && presentedFrames >= 3 && drawCalls > 0 && triangles > 0 && boundsInFrustum) {
                            reportedFrame = true
                            recordSmokeResult("FRAME_PRESENTED", apiVersion, frames, drawCalls, triangles,
                                presentedFrames, boundsInFrustum, outsideCorners, glError, eglError)
                            Log.i(TAG, "FRAME_PRESENTED api=$apiVersion frames=$frames presented=$presentedFrames draws=$drawCalls triangles=$triangles boundsInFrustum=$boundsInFrustum")
                            onStatus("$backendLabel frame presented · $presentedFrames swaps · $drawCalls draws · $triangles tris")
                        } else if (System.nanoTime() - frameWaitStarted > firstPresentBudgetNanos) {
                            error("No drawable $backendLabel-presented model frame within $firstPresentBudgetSeconds seconds " +
                                "(frames=$frames presented=$presentedFrames framePresented=$framePresented " +
                                "draws=$drawCalls triangles=$triangles boundsInFrustum=$boundsInFrustum " +
                                "outsideCorners=$outsideCorners glError=0x${glError.toString(16)} eglError=0x${eglError.toString(16)})")
                        }
                    }
                    if (verifyVisiblePixels && presentedThisFrame && presentedFrames >= 3 &&
                        drawCalls > 0 && triangles > 0 && boundsInFrustum && !pixelCopyComplete.get()) {
                        // Finish GPU work for the presented frame and hold the next frame until
                        // PixelCopy completes. On software SwiftShader emulators, submitting new
                        // frames back-to-back starves SurfaceFlinger's PixelCopy readback until
                        // the 15s timeout fires.
                        nativeWaitIdle(nativeHandle)
                        requestVisiblePixelCopy(apiVersion, frames, drawCalls, triangles, presentedFrames,
                            boundsInFrustum, outsideCorners, glError, eglError)
                        while (pixelCopyPending.get() && running.get() && surfaceReady) {
                            Thread.sleep(5)
                        }
                    }
                    val remaining = 16_666_667L - (System.nanoTime() - now)
                    if (remaining > 0L) Thread.sleep(remaining / 1_000_000L, (remaining % 1_000_000L).toInt())
                }
            } catch (failure: Throwable) {
                val message = failure.message ?: failure.javaClass.simpleName
                failureStatus = "AvatarForge renderer failed: $message"
                recordSmokeResult("FAILED", error = message)
                Log.e(TAG, "Renderer failed: $message", failure)
            } finally {
                val cleanupFailure = if (nativeHandle != 0L) {
                    runCatching { nativeDestroy(nativeHandle) }.exceptionOrNull()
                } else {
                    null
                }
                running.set(false)
                if (renderThread === Thread.currentThread()) renderThread = null

                // Do not overwrite the actual renderer failure with the generic stopped state.
                // The instrumentation gate and the app status panel need the first real cause.
                val terminalMessages = mutableListOf<String>()
                failureStatus?.let(terminalMessages::add)
                cleanupFailure?.let {
                    terminalMessages.add("AvatarForge cleanup failed: ${it.message ?: it.javaClass.simpleName}")
                }
                val finalStatus = when {
                    terminalMessages.isNotEmpty() -> terminalMessages.joinToString("; ")
                    surfaceReady -> "AvatarForge renderer stopped."
                    else -> "AvatarForge surface released."
                }
                onStatus(finalStatus)
            }
        }, "AvatarForge-GLES-Render")
        worker.priority = Thread.NORM_PRIORITY
        renderThread = worker
        worker.start()
    }

    private fun recordSmokeResult(
        status: String,
        apiVersion: String = "",
        frames: Int = 0,
        drawCalls: Int = 0,
        triangles: Int = 0,
        presentedFrames: Int = 0,
        boundsInFrustum: Boolean = false,
        outsideCorners: Int = 0,
        glError: Int = 0,
        eglError: Int = 0,
        pixelCount: Int = 0,
        screenshotPath: String = "",
        error: String = "",
        backend: String = ""
    ) {
        val preferences = context.getSharedPreferences(SMOKE_PREFS, Context.MODE_PRIVATE)
        val editor = preferences.edit()
            .putString("status", status)
            .putString("api_version", apiVersion)
            .putInt("frames", frames)
            .putInt("draw_calls", drawCalls)
            .putInt("triangles", triangles)
            .putInt("presented_frames", presentedFrames)
            .putBoolean("bounds_in_frustum", boundsInFrustum)
            .putInt("outside_corners", outsideCorners)
            .putInt("gl_error", glError)
            .putInt("egl_error", eglError)
            .putInt("pixel_count", pixelCount)
            .putString("screenshot_path", screenshotPath)
            .putString("error", error)
            .putLong("updated_at_epoch_ms", System.currentTimeMillis())
        if (backend.isNotEmpty()) {
            editor.putString("backend", backend)
        }
        editor.commit()

        // Keep a durable, machine-readable report in Downloads, not only SharedPreferences.
        val report = org.json.JSONObject()
            .put("schema", "com.davemega.hub.renderer-diagnostics.v1")
            .put("updatedAtEpochMs", System.currentTimeMillis())
            .put("asset", glbFile.name)
            .put("assetBytes", glbFile.length())
            .put("status", status)
            .put("backend", "OpenGL ES")
            .put("apiVersion", apiVersion)
            .put("surfaceWidth", surfaceWidth)
            .put("surfaceHeight", surfaceHeight)
            .put("frames", frames)
            .put("drawCalls", drawCalls)
            .put("triangles", triangles)
            .put("presentedFrames", presentedFrames)
            .put("boundsInFrustum", boundsInFrustum)
            .put("outsideCorners", outsideCorners)
            .put("glError", "0x" + glError.toString(16))
            .put("eglError", "0x" + eglError.toString(16))
            .put("visiblePixels", pixelCount)
            .put("minimumVisiblePixels", MIN_VISIBLE_PIXELS)
            .put("screenshotPathAppPrivate", screenshotPath)
            .put("screenshotDownloadsUri", preferences.getString("downloads_screenshot_uri", null))
            .put("error", error)
            .put("downloadsDirectory", DiagnosticDownloads.locationLabel())
        runCatching {
            val uri = DiagnosticDownloads.save(
                context,
                "renderer_diagnostics.json",
                "application/json"
            ) { output ->
                output.write(report.toString(2).toByteArray(Charsets.UTF_8))
            }
            preferences.edit()
                .putString("downloads_report_uri", uri.toString())
                .remove("downloads_report_error")
                .commit()
            Log.i(TAG, "Diagnostic report saved: $uri")
        }.onFailure { failure ->
            preferences.edit()
                .putString("downloads_report_error", failure.message ?: failure.javaClass.simpleName)
                .commit()
            Log.e(TAG, "Could not save diagnostic report to Downloads", failure)
        }
    }

    private fun requestVisiblePixelCopy(
        apiVersion: String,
        frames: Int,
        drawCalls: Int,
        triangles: Int,
        presentedFrames: Int,
        boundsInFrustum: Boolean,
        outsideCorners: Int,
        glError: Int,
        eglError: Int
    ) {
        if (!verifyVisiblePixels || pixelCopyComplete.get() ||
            !pixelCopyPending.compareAndSet(false, true)) return
        val attempt = pixelCopyAttempts.incrementAndGet()
        val posted = mainHandler.post {
            if (!surfaceReady || pixelCopyComplete.get()) {
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                return@post
            }
            pixelCopyActiveAttempt.set(attempt)
            val captureWidth = width
            val captureHeight = height
            if (captureWidth <= 0 || captureHeight <= 0 ||
                captureWidth.toLong() * captureHeight.toLong() > Int.MAX_VALUE.toLong()) {
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                if (attempt >= MAX_PIXEL_COPY_ATTEMPTS) {
                    pixelCopyComplete.set(true)
                    val failure = "FAILED: PixelCopy has invalid SurfaceView dimensions after $attempt attempts"
                    recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                        presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = failure)
                    onStatus(failure)
                }
                return@post
            }

            val bitmap = runCatching {
                Bitmap.createBitmap(captureWidth, captureHeight, Bitmap.Config.ARGB_8888)
            }.getOrElse { failure ->
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                pixelCopyComplete.set(true)
                val message = "FAILED: could not allocate PixelCopy bitmap: ${failure.message}"
                recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                    presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                onStatus(message)
                return@post
            }

            val bitmapReleased = AtomicBoolean(false)
            fun recycleBitmap() {
                if (bitmapReleased.compareAndSet(false, true)) bitmap.recycle()
            }
            val timeout = Runnable {
                if (pixelCopyActiveAttempt.compareAndSet(attempt, 0) &&
                    pixelCopyPending.compareAndSet(true, false)) {
                    pixelCopyComplete.set(true)
                    recycleBitmap()
                    val message = "FAILED: PixelCopy callback timed out after ${PIXEL_COPY_TIMEOUT_MS / 1000L}s"
                    recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                        presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                    onStatus(message)
                }
            }
            mainHandler.postDelayed(timeout, PIXEL_COPY_TIMEOUT_MS)

            try {
                PixelCopy.request(this, bitmap, { copyResult ->
                    /* A late callback from a timed-out or destroyed surface must never
                       complete a newer request or overwrite its diagnostic result. */
                    if (pixelCopyActiveAttempt.get() != attempt ||
                        !pixelCopyPending.compareAndSet(true, false)) {
                        recycleBitmap()
                        return@request
                    }
                    mainHandler.removeCallbacks(timeout)
                    pixelCopyActiveAttempt.compareAndSet(attempt, 0)

                    if (copyResult != PixelCopy.SUCCESS) {
                        recycleBitmap()
                        if (attempt >= MAX_PIXEL_COPY_ATTEMPTS) {
                            pixelCopyComplete.set(true)
                            val message = "FAILED: PixelCopy returned result=$copyResult after $attempt attempts"
                            recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                                presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                            onStatus(message)
                        } else {
                            onStatus("PixelCopy attempt $attempt failed (result=$copyResult); retrying after next presented frame")
                        }
                        return@request
                    }

                    val pixels = IntArray(captureWidth * captureHeight)
                    bitmap.getPixels(pixels, 0, captureWidth, 0, 0, captureWidth, captureHeight)
                    /* Estimate the actual clear colour from the four corners. A dark
                       blue-grey background must not count as avatar geometry. */
                    val corners = intArrayOf(
                        pixels[0],
                        pixels[captureWidth - 1],
                        pixels[(captureHeight - 1) * captureWidth],
                        pixels[pixels.lastIndex]
                    )
                    fun medianChannel(channel: (Int) -> Int): Int {
                        val values = corners.map(channel).sorted()
                        return (values[1] + values[2]) / 2
                    }
                    val backgroundRed = medianChannel(Color::red)
                    val backgroundGreen = medianChannel(Color::green)
                    val backgroundBlue = medianChannel(Color::blue)
                    val visiblePixels = pixels.count { pixel ->
                        val redDelta = abs(Color.red(pixel) - backgroundRed)
                        val greenDelta = abs(Color.green(pixel) - backgroundGreen)
                        val blueDelta = abs(Color.blue(pixel) - backgroundBlue)
                        redDelta + greenDelta + blueDelta >= 48 &&
                            maxOf(redDelta, greenDelta, blueDelta) >= 16
                    }
                    val passed = visiblePixels >= MIN_VISIBLE_PIXELS
                    val generatedDiagnostic = glbFile.name.startsWith("diagnostic-seed-1337-")
                    val outputName = when {
                        !passed -> "failed_render.png"
                        generatedDiagnostic -> "generated_avatar_diagnostic.png"
                        else -> "imported_glb_diagnostic.png"
                    }
                    val outputFile = context.getExternalFilesDir(null)?.let { File(it, outputName) }
                    val saved = outputFile != null && runCatching {
                        FileOutputStream(outputFile).use { stream ->
                            check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream)) {
                                "Bitmap PNG compression failed"
                            }
                        }
                    }.isSuccess
                    recycleBitmap()
                    pixelCopyComplete.set(true)
                    val screenshot = if (saved) outputFile!!.absolutePath else "UNAVAILABLE"
                    val downloadsScreenshotUri = if (saved) runCatching {
                        DiagnosticDownloads.save(context, outputName, "image/png") { output ->
                            outputFile!!.inputStream().buffered().use { it.copyTo(output) }
                        }
                    }.onFailure { failure ->
                        Log.e(TAG, "Could not save renderer screenshot to Downloads", failure)
                    }.getOrNull() else null
                    context.getSharedPreferences(SMOKE_PREFS, Context.MODE_PRIVATE).edit()
                        .putString("downloads_screenshot_uri", downloadsScreenshotUri?.toString())
                        .commit()
                    val downloadsLabel = downloadsScreenshotUri?.let { "; saved to ${DiagnosticDownloads.locationLabel()}/$outputName" }
                        ?: "; Downloads screenshot export failed (see renderer_diagnostics.json)"
                    val message = if (passed) {
                        "REAL + VERIFIED: PixelCopy saw $visiblePixels geometry pixels (background RGB=$backgroundRed,$backgroundGreen,$backgroundBlue) after $presentedFrames successful EGL swaps; screenshot=$screenshot$downloadsLabel"
                    } else {
                        "FAILED: PixelCopy saw only $visiblePixels geometry pixels after background subtraction (required >=$MIN_VISIBLE_PIXELS); screenshot=$screenshot$downloadsLabel"
                    }
                    recordSmokeResult(if (passed) "PIXELS_VERIFIED" else "FAILED",
                        apiVersion, frames, drawCalls, triangles, presentedFrames, boundsInFrustum,
                        outsideCorners, glError, eglError, visiblePixels, screenshot,
                        if (passed) "" else message)
                    onStatus(message)
                }, mainHandler)
            } catch (failure: Throwable) {
                mainHandler.removeCallbacks(timeout)
                pixelCopyActiveAttempt.compareAndSet(attempt, 0)
                pixelCopyPending.set(false)
                pixelCopyComplete.set(true)
                recycleBitmap()
                val message = "FAILED: PixelCopy request threw ${failure.message ?: failure.javaClass.simpleName}"
                recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                    presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
                onStatus(message)
            }
        }
        if (!posted) {
            pixelCopyActiveAttempt.compareAndSet(attempt, 0)
            pixelCopyPending.set(false)
            pixelCopyComplete.set(true)
            val message = "FAILED: main thread rejected the PixelCopy request"
            recordSmokeResult("FAILED", apiVersion, frames, drawCalls, triangles,
                presentedFrames, boundsInFrustum, outsideCorners, glError, eglError, error = message)
            onStatus(message)
        }
    }
    private external fun nativeCreate(
        surface: android.view.Surface,
        glbPath: String,
        width: Int,
        height: Int,
        preferVulkan: Boolean,
        allowLiveRig: Boolean
    ): Long
    private external fun nativeResize(handle: Long, width: Int, height: Int)
    private external fun nativeOrbitCamera(handle: Long, deltaYawDeg: Float, deltaPitchDeg: Float, zoomScale: Float)
    private external fun nativeResetCamera(handle: Long)
    private external fun nativeSetCameraPreset(handle: Long, presetId: Int)
    private external fun nativeSetLightingPreset(handle: Long, presetId: Int)
    private external fun nativeSetInspectionModes(handle: Long, showSkeleton: Boolean, flatShading: Boolean)
    private external fun nativeGetCapabilities(handle: Long): IntArray
    private external fun nativeSetAnimationClip(handle: Long, clipId: Int, speed: Float, loop: Boolean)
    private external fun nativeApplyExpression(handle: Long, expressionId: Int)
    private external fun nativePickRegion(handle: Long, normX: Float, normY: Float): FloatArray
    private external fun nativeClearHighlight(handle: Long)
    private external fun nativeRender(handle: Long, deltaSeconds: Float): Boolean
    private external fun nativeWaitIdle(handle: Long)
    private external fun nativeGetInfo(handle: Long): String
    private external fun nativeGetStats(handle: Long): IntArray
    private external fun nativeDestroy(handle: Long)

    companion object {
        private const val TAG = "AvatarForge"
        private const val SMOKE_PREFS = "avatarforge_renderer_smoke"
        private const val MIN_VISIBLE_PIXELS = 5_000
        private const val MAX_PIXEL_COPY_ATTEMPTS = 3
        private const val PIXEL_COPY_TIMEOUT_MS = 15_000L
        init { System.loadLibrary("mega_hub_avatarforge") }
    }
}

