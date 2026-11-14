package com.davemega.hub.avatarforge

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Debug
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

/**
 * One render worker owns the AvatarForge engine, renderer and ANativeWindow lifetime.
 *
 * [onTelemetry] receives a measured [RendererTelemetry] snapshot about once per second, on the
 * render thread. [onStatus] receives human-readable state changes; a failure message is the final
 * status of a failed session and is never replaced by a generic "stopped" line.
 */
class AvatarForgeSurfaceView(
    context: Context,
    private val glbFile: File,
    private val verifyVisiblePixels: Boolean = false,
    private val onTelemetry: ((RendererTelemetry) -> Unit)? = null,
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
    private var lastTouchX = 0f
    private var lastTouchY = 0f
    private var activePointerId = MotionEvent.INVALID_POINTER_ID

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
            var terminalFailure: String? = null
            var session: TelemetrySession? = null
            try {
                recordSmokeResult("STARTING")
                onStatus("Starting AvatarForge GLES renderer and importing ${glbFile.name}…")
                while (running.get() && surfaceReady && (surfaceWidth <= 0 || surfaceHeight <= 0)) Thread.sleep(10)
                if (!running.get() || !surfaceReady) return@Thread
                check(glbFile.isFile && glbFile.length() > 0L) { "Selected GLB is not available in app storage." }
                Log.i(TAG, "nativeCreate GLB=${glbFile.absolutePath} size=${surfaceWidth}x${surfaceHeight}")
                nativeHandle = nativeCreate(surfaceHolder.surface, glbFile.absolutePath, max(1, surfaceWidth), max(1, surfaceHeight))
                check(nativeHandle != 0L) { "AvatarForge native renderer returned a null session." }
                val rendererInfo = nativeGetInfo(nativeHandle)
                check(rendererInfo.startsWith("gles31|")) {
                    "Unexpected AvatarForge backend information: $rendererInfo"
                }
                val apiVersion = rendererInfo.substringAfter('|').substringBefore('|')
                val telemetry = TelemetrySession(
                    backend = "OpenGL ES",
                    apiVersion = apiVersion,
                    deviceName = rendererInfo.substringAfter('|').substringAfter('|', "")
                )
                session = telemetry
                recordSmokeResult("RENDERER_ACTIVE", apiVersion = apiVersion)
                onStatus("AvatarForge GLES renderer active · $apiVersion · ${glbFile.name}")
                var lastFrame = System.nanoTime()
                var lastWidth = surfaceWidth
                var lastHeight = surfaceHeight
                var reportedFrame = false
                var slowStartReported = false
                var stallReported = false
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
                    synchronized(cameraLock) {
                        deltaYaw = pendingYawDeg
                        deltaPitch = pendingPitchDeg
                        zoomScale = pendingZoomScale
                        resetCam = pendingCameraReset
                        pendingYawDeg = 0f
                        pendingPitchDeg = 0f
                        pendingZoomScale = 1f
                        pendingCameraReset = false
                    }
                    if (resetCam) {
                        nativeResetCamera(nativeHandle)
                    } else if (deltaYaw != 0f || deltaPitch != 0f || zoomScale != 1f) {
                        nativeOrbitCamera(nativeHandle, deltaYaw, deltaPitch, zoomScale)
                    }
                    val now = System.nanoTime()
                    val delta = ((now - lastFrame).toDouble() / 1_000_000_000.0).coerceIn(0.001, 0.1).toFloat()
                    lastFrame = now
                    val presentedThisFrame = nativeRender(nativeHandle, delta)
                    val renderReturned = System.nanoTime()
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
                    /* A valid frame is one that was presented AND drew in-frustum model geometry. */
                    val validFrame = presentedThisFrame && framePresented && drawCalls > 0 && triangles > 0 && boundsInFrustum
                    telemetry.recordFrame(now, renderReturned, presentedThisFrame && framePresented, validFrame)
                    val sinceStartNs = renderReturned - frameWaitStarted
                    if (!reportedFrame) {
                        if (validFrame && presentedFrames >= 3) {
                            reportedFrame = true
                            recordSmokeResult("FRAME_PRESENTED", apiVersion, frames, drawCalls, triangles,
                                presentedFrames, boundsInFrustum, outsideCorners, glError, eglError)
                            Log.i(TAG, "FRAME_PRESENTED api=$apiVersion frames=$frames presented=$presentedFrames draws=$drawCalls triangles=$triangles boundsInFrustum=$boundsInFrustum")
                            onStatus("EGL frame presented · $presentedFrames successful swaps · $drawCalls draw calls · $triangles triangles")
                        } else if (telemetry.validFrames == 0L && sinceStartNs > NO_VALID_FRAME_TIMEOUT_NS) {
                            /* Frames are coming back from the renderer but none of them is a valid,
                               presented model frame. That is a broken renderer, not a slow one. */
                            error("No drawable EGL-presented model frame within ${NO_VALID_FRAME_TIMEOUT_NS / 1_000_000_000L} seconds " +
                                "(frames=$frames presented=$presentedFrames framePresented=$framePresented " +
                                "draws=$drawCalls triangles=$triangles boundsInFrustum=$boundsInFrustum " +
                                "outsideCorners=$outsideCorners glError=0x${glError.toString(16)} eglError=0x${eglError.toString(16)})")
                        } else if (!slowStartReported && sinceStartNs > NO_VALID_FRAME_TIMEOUT_NS) {
                            /* Valid frames exist but fewer than three arrived in the first window.
                               The renderer works and is slow; say so with the measured numbers and
                               keep rendering instead of tearing a working session down. */
                            slowStartReported = true
                            val lastMs = (renderReturned - now) / 1_000_000L
                            Log.w(TAG, "SLOW_START validFrames=${telemetry.validFrames} elapsedMs=${sinceStartNs / 1_000_000L} lastRenderCallMs=$lastMs")
                            onStatus("Renderer is presenting slowly: ${telemetry.validFrames} valid frame(s) in " +
                                "${sinceStartNs / 1_000_000_000L} s, last render call $lastMs ms · $drawCalls draw calls · $triangles triangles. Still rendering.")
                        }
                    } else if (telemetry.nanosSinceLastValidFrame(renderReturned) > STALL_TIMEOUT_NS) {
                        if (!stallReported) {
                            stallReported = true
                            telemetry.stallEvents++
                            val stalledSeconds = telemetry.nanosSinceLastValidFrame(renderReturned) / 1_000_000_000L
                            Log.e(TAG, "STALL no valid frame for ${stalledSeconds}s frames=$frames presented=$presentedFrames draws=$drawCalls glError=0x${glError.toString(16)} eglError=0x${eglError.toString(16)}")
                            onStatus("FAILED: no valid frame presented for $stalledSeconds s " +
                                "(framePresented=$framePresented draws=$drawCalls triangles=$triangles boundsInFrustum=$boundsInFrustum " +
                                "glError=0x${glError.toString(16)} eglError=0x${eglError.toString(16)}). Renderer is still retrying.")
                        }
                    } else if (stallReported && validFrame) {
                        stallReported = false
                        onStatus("Renderer recovered: valid frames are being presented again · $drawCalls draw calls · $triangles triangles")
                    }
                    if (telemetry.shouldPublish(renderReturned)) {
                        val metrics = nativeGetMetrics(nativeHandle)
                        val snapshot = telemetry.snapshot(renderReturned, metrics)
                        latestTelemetry = snapshot
                        /* A diagnostics listener must never be able to stop the renderer. */
                        runCatching { onTelemetry?.invoke(snapshot) }
                            .onFailure { Log.e(TAG, "Telemetry listener failed", it) }
                    }
                    if (verifyVisiblePixels && presentedThisFrame && presentedFrames >= 3 &&
                        drawCalls > 0 && triangles > 0 && boundsInFrustum && !pixelCopyComplete.get()) {
                        requestVisiblePixelCopy(apiVersion, frames, drawCalls, triangles, presentedFrames,
                            boundsInFrustum, outsideCorners, glError, eglError)
                    }
                    val remaining = 16_666_667L - (System.nanoTime() - now)
                    if (remaining > 0L) Thread.sleep(remaining / 1_000_000L, (remaining % 1_000_000L).toInt())
                }
            } catch (failure: Throwable) {
                val message = failure.message ?: failure.javaClass.simpleName
                recordSmokeResult("FAILED", error = message)
                Log.e(TAG, "Renderer failed: $message", failure)
                terminalFailure = "AvatarForge renderer failed: $message"
                onStatus("AvatarForge renderer failed: $message")
            } finally {
                /* Take the final measured snapshot while the native session still exists. */
                val finalSnapshot = if (nativeHandle != 0L) {
                    runCatching {
                        session?.snapshot(System.nanoTime(), nativeGetMetrics(nativeHandle))
                    }.getOrNull()
                } else null
                var cleanupFailure: String? = null
                if (nativeHandle != 0L) runCatching { nativeDestroy(nativeHandle) }
                    .onFailure { cleanupFailure = "AvatarForge cleanup failed: ${it.message}" }
                if (finalSnapshot != null) {
                    latestTelemetry = finalSnapshot
                    runCatching { onTelemetry?.invoke(finalSnapshot) }
                    persistTelemetry(finalSnapshot, terminalFailure ?: cleanupFailure)
                }
                running.set(false)
                if (renderThread === Thread.currentThread()) renderThread = null
                /* The reason a session ended must remain the last status. Reporting a generic
                   "stopped" line here used to overwrite the failure message, which is how the
                   production-surface CI test came to fail with no visible cause. */
                onStatus(
                    terminalFailure
                        ?: cleanupFailure
                        ?: if (surfaceReady) "AvatarForge renderer stopped." else "AvatarForge surface released."
                )
            }
        }, "AvatarForge-GLES-Render")
        worker.priority = Thread.NORM_PRIORITY
        renderThread = worker
        worker.start()
    }

    /** Render-thread-only accumulator behind [RendererTelemetry]. */
    private inner class TelemetrySession(
        private val backend: String,
        private val apiVersion: String,
        private val deviceName: String
    ) {
        private val startedNs = System.nanoTime()
        private val presentIntervals = FrameSampleWindow(FRAME_WINDOW)
        private val renderCalls = FrameSampleWindow(FRAME_WINDOW)
        private var lastPresentNs = 0L
        private var lastValidNs = 0L
        private var lastPublishNs = 0L
        private var framesRequested = 0L
        private var framesPresented = 0L
        private var framesNotPresented = 0L
        private var framesWithoutValidContent = 0L
        var validFrames = 0L
            private set
        var stallEvents = 0

        fun recordFrame(callStartNs: Long, callEndNs: Long, presented: Boolean, valid: Boolean) {
            framesRequested++
            renderCalls.add(callEndNs - callStartNs)
            if (presented) {
                framesPresented++
                if (lastPresentNs != 0L) presentIntervals.add(callEndNs - lastPresentNs)
                lastPresentNs = callEndNs
            } else {
                framesNotPresented++
            }
            if (valid) {
                validFrames++
                lastValidNs = callEndNs
            } else {
                framesWithoutValidContent++
            }
        }

        fun nanosSinceLastValidFrame(nowNs: Long): Long =
            if (lastValidNs == 0L) nowNs - startedNs else nowNs - lastValidNs

        fun shouldPublish(nowNs: Long): Boolean {
            if (lastPublishNs != 0L && nowNs - lastPublishNs < TELEMETRY_PERIOD_NS) return false
            lastPublishNs = nowNs
            return true
        }

        fun snapshot(nowNs: Long, metrics: LongArray?): RendererTelemetry {
            check(metrics != null && metrics.size == METRIC_COUNT && metrics[0] == METRIC_LAYOUT_VERSION) {
                "AvatarForge returned an unexpected renderer metric layout (size=${metrics?.size}, version=${metrics?.getOrNull(0)})."
            }
            fun millis(index: Int): Double = if (metrics[index] < 0L) -1.0 else metrics[index] / 1000.0
            val runtime = Runtime.getRuntime()
            return RendererTelemetry(
                backend = backend,
                apiVersion = apiVersion,
                deviceName = deviceName,
                assetName = glbFile.name,
                assetBytes = glbFile.length(),
                surfaceWidth = surfaceWidth,
                surfaceHeight = surfaceHeight,
                uptimeMs = (nowNs - startedNs) / 1_000_000L,
                framesRequested = framesRequested,
                framesPresented = framesPresented,
                framesNotPresented = framesNotPresented,
                framesWithoutValidContent = framesWithoutValidContent,
                stallEvents = stallEvents,
                frameSamples = presentIntervals.count(),
                fps = presentIntervals.ratePerSecond(),
                frameIntervalP50Ms = presentIntervals.percentileMs(50.0),
                frameIntervalP95Ms = presentIntervals.percentileMs(95.0),
                frameIntervalP99Ms = presentIntervals.percentileMs(99.0),
                frameIntervalMaxMs = presentIntervals.maxMs(),
                renderCallP50Ms = renderCalls.percentileMs(50.0),
                renderCallP95Ms = renderCalls.percentileMs(95.0),
                renderCallMaxMs = renderCalls.maxMs(),
                lastCpuMs = millis(6),
                lastSwapMs = millis(7),
                drawCalls = metrics[11].toInt(),
                trianglesDrawn = metrics[10].toInt(),
                verticesUploaded = metrics[9].toInt(),
                modelVertices = metrics[26].toInt(),
                modelTriangles = metrics[27].toInt(),
                submeshes = metrics[28].toInt(),
                joints = metrics[29].toInt(),
                glbImportMs = millis(1),
                initialPoseMs = millis(2),
                rendererInitMs = millis(3),
                nativeCreateMs = millis(4),
                firstFrameMs = millis(5),
                textureBytes = metrics[12],
                geometryBytes = metrics[13],
                renderTargetBytes = metrics[14],
                msaaSamples = metrics[15].toInt(),
                shadowMapSize = metrics[16].toInt(),
                bloomLevels = metrics[17].toInt(),
                qualityLevel = metrics[18].toInt(),
                glErrorFrames = metrics[19].toInt(),
                lastGlError = metrics[20].toInt(),
                eglErrorFrames = metrics[21].toInt(),
                failedDrawCalls = metrics[22].toInt(),
                cameraYawDeg = metrics[23] / 1000.0,
                cameraPitchDeg = metrics[24] / 1000.0,
                cameraDistanceM = metrics[25] / 1_000_000.0,
                nativeHeapBytes = Debug.getNativeHeapAllocatedSize(),
                javaHeapBytes = runtime.totalMemory() - runtime.freeMemory(),
                nativeSessionsCreated = metrics[30],
                nativeSessionsDestroyed = metrics[31]
            )
        }
    }

    /**
     * Writes the final measured snapshot of a render session next to the renderer report:
     * app-private external files (readable with adb / run-as) and the public Downloads
     * diagnostics folder. A write failure is logged and never hides the session result.
     */
    private fun persistTelemetry(snapshot: RendererTelemetry, failure: String?) {
        val report = snapshot.toJson()
            .put("updatedAtEpochMs", System.currentTimeMillis())
            .put("sessionResult", if (failure == null) "ENDED" else "FAILED")
            .put("failure", failure ?: "")
        val text = runCatching { report.toString(2) }.getOrElse { failure2 ->
            Log.e(TAG, "Could not serialise renderer telemetry", failure2)
            return
        }
        runCatching {
            context.getExternalFilesDir(null)?.let { directory ->
                File(directory, TELEMETRY_FILE).writeText(text, Charsets.UTF_8)
            }
        }.onFailure { Log.e(TAG, "Could not save renderer telemetry to app files", it) }
        runCatching {
            DiagnosticDownloads.save(context, TELEMETRY_FILE, "application/json") { output ->
                output.write(text.toByteArray(Charsets.UTF_8))
            }
        }.onFailure { Log.e(TAG, "Could not save renderer telemetry to Downloads", it) }
        Log.i(TAG, "TELEMETRY " + report.toString())
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
        error: String = ""
    ) {
        val preferences = context.getSharedPreferences(SMOKE_PREFS, Context.MODE_PRIVATE)
        preferences.edit()
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
            .commit()

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
            /* Latest measured snapshot (at most about one second old); null before the first frame. */
            .put("telemetry", latestTelemetry?.toJson() ?: org.json.JSONObject.NULL)
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
    private external fun nativeCreate(surface: android.view.Surface, glbPath: String, width: Int, height: Int): Long
    private external fun nativeResize(handle: Long, width: Int, height: Int)
    private external fun nativeOrbitCamera(handle: Long, deltaYawDeg: Float, deltaPitchDeg: Float, zoomScale: Float)
    private external fun nativeResetCamera(handle: Long)
    private external fun nativeRender(handle: Long, deltaSeconds: Float): Boolean
    private external fun nativeGetInfo(handle: Long): String
    private external fun nativeGetStats(handle: Long): IntArray
    private external fun nativeGetMetrics(handle: Long): LongArray?
    private external fun nativeDestroy(handle: Long)

    companion object {
        private const val TAG = "AvatarForge"
        private const val SMOKE_PREFS = "avatarforge_renderer_smoke"
        private const val MIN_VISIBLE_PIXELS = 5_000
        private const val MAX_PIXEL_COPY_ATTEMPTS = 3
        private const val PIXEL_COPY_TIMEOUT_MS = 15_000L
        /** Fail only when frames return for this long without a single valid presented frame. */
        private const val NO_VALID_FRAME_TIMEOUT_NS = 15_000_000_000L
        /** After a session has rendered, report (not kill) a gap this long with no valid frame. */
        private const val STALL_TIMEOUT_NS = 15_000_000_000L
        private const val TELEMETRY_PERIOD_NS = 1_000_000_000L
        private const val FRAME_WINDOW = 240
        /** Must match nativeGetMetrics in avatarforge_renderer_jni.cpp. */
        private const val METRIC_COUNT = 32
        private const val METRIC_LAYOUT_VERSION = 1L
        const val TELEMETRY_FILE = "renderer_telemetry.json"

        /**
         * Most recent measured snapshot from any AvatarForge render session in this process,
         * including the final snapshot of a session that has ended. Null until a renderer has
         * produced one. Diagnostics screens and instrumentation read this instead of guessing.
         */
        @Volatile
        var latestTelemetry: RendererTelemetry? = null
            private set

        init { System.loadLibrary("mega_hub_avatarforge") }
    }
}

