package com.davemega.hub.avatarforge

import org.json.JSONObject
import java.util.Locale
import kotlin.math.ceil

/**
 * Fixed-size window of nanosecond samples used for frame-time statistics.
 *
 * It stores what was measured and nothing else: an empty window reports 0 samples and 0.0 for
 * every statistic, so a caller can never mistake "not measured yet" for "fast".
 */
class FrameSampleWindow(capacity: Int) {
    private val samples = LongArray(capacity.coerceAtLeast(1))
    private var size = 0
    private var next = 0

    fun add(valueNs: Long) {
        samples[next] = valueNs
        next = (next + 1) % samples.size
        if (size < samples.size) size++
    }

    fun count(): Int = size

    fun sumNs(): Long {
        var total = 0L
        for (index in 0 until size) total += samples[index]
        return total
    }

    /** Nearest-rank percentile in milliseconds. [percentile] is 0..100. */
    fun percentileMs(percentile: Double): Double {
        if (size == 0) return 0.0
        val sorted = samples.copyOf(size)
        sorted.sort()
        val rank = ceil(percentile / 100.0 * size).toInt().coerceIn(1, size)
        return sorted[rank - 1] / 1_000_000.0
    }

    fun maxMs(): Double {
        if (size == 0) return 0.0
        var largest = samples[0]
        for (index in 1 until size) if (samples[index] > largest) largest = samples[index]
        return largest / 1_000_000.0
    }

    /** Mean rate over the window: samples per second of accumulated sample time. */
    fun ratePerSecond(): Double {
        val total = sumNs()
        return if (size == 0 || total <= 0L) 0.0 else size * 1_000_000_000.0 / total
    }
}

/**
 * One measured snapshot of the native AvatarForge GLES renderer.
 *
 * Nothing in this class is estimated from a nominal refresh rate or from the frame delta handed
 * to the animation system. Frame statistics come from `System.nanoTime()` around the native render
 * call and between successful EGL presents; native durations come from `steady_clock` inside the
 * renderer; counts come from the renderer's own statistics. Durations that have not been measured
 * are reported as -1.0, and GPU byte figures are the sizes the renderer requested from the driver,
 * not physical GPU memory.
 */
data class RendererTelemetry(
    val backend: String,
    val apiVersion: String,
    val deviceName: String,
    val assetName: String,
    val assetBytes: Long,
    val surfaceWidth: Int,
    val surfaceHeight: Int,
    val uptimeMs: Long,
    val framesRequested: Long,
    val framesPresented: Long,
    val framesNotPresented: Long,
    val framesWithoutValidContent: Long,
    val stallEvents: Int,
    val frameSamples: Int,
    val fps: Double,
    val frameIntervalP50Ms: Double,
    val frameIntervalP95Ms: Double,
    val frameIntervalP99Ms: Double,
    val frameIntervalMaxMs: Double,
    val renderCallP50Ms: Double,
    val renderCallP95Ms: Double,
    val renderCallMaxMs: Double,
    val lastCpuMs: Double,
    val lastSwapMs: Double,
    val drawCalls: Int,
    val trianglesDrawn: Int,
    val verticesUploaded: Int,
    val modelVertices: Int,
    val modelTriangles: Int,
    val submeshes: Int,
    val joints: Int,
    val glbImportMs: Double,
    val initialPoseMs: Double,
    val rendererInitMs: Double,
    val nativeCreateMs: Double,
    val firstFrameMs: Double,
    val textureBytes: Long,
    val geometryBytes: Long,
    val renderTargetBytes: Long,
    val msaaSamples: Int,
    val shadowMapSize: Int,
    val bloomLevels: Int,
    val qualityLevel: Int,
    val glErrorFrames: Int,
    val lastGlError: Int,
    val eglErrorFrames: Int,
    val failedDrawCalls: Int,
    val cameraYawDeg: Double,
    val cameraPitchDeg: Double,
    val cameraDistanceM: Double,
    val nativeHeapBytes: Long,
    val javaHeapBytes: Long,
    val nativeSessionsCreated: Long,
    val nativeSessionsDestroyed: Long
) {
    val errorCount: Int
        get() = glErrorFrames + eglErrorFrames + failedDrawCalls

    fun toJson(): JSONObject = JSONObject()
        .put("schema", SCHEMA)
        .put("backend", backend)
        .put("apiVersion", apiVersion)
        .put("deviceName", deviceName)
        .put("asset", assetName)
        .put("assetBytes", assetBytes)
        .put("surfaceWidth", surfaceWidth)
        .put("surfaceHeight", surfaceHeight)
        .put("uptimeMs", uptimeMs)
        .put("framesRequested", framesRequested)
        .put("framesPresented", framesPresented)
        .put("framesNotPresented", framesNotPresented)
        .put("framesWithoutValidContent", framesWithoutValidContent)
        .put("stallEvents", stallEvents)
        .put("frameSamples", frameSamples)
        .put("fps", finite(fps))
        .put("frameIntervalP50Ms", finite(frameIntervalP50Ms))
        .put("frameIntervalP95Ms", finite(frameIntervalP95Ms))
        .put("frameIntervalP99Ms", finite(frameIntervalP99Ms))
        .put("frameIntervalMaxMs", finite(frameIntervalMaxMs))
        .put("renderCallP50Ms", finite(renderCallP50Ms))
        .put("renderCallP95Ms", finite(renderCallP95Ms))
        .put("renderCallMaxMs", finite(renderCallMaxMs))
        .put("lastCpuMs", finite(lastCpuMs))
        .put("lastSwapMs", finite(lastSwapMs))
        .put("drawCalls", drawCalls)
        .put("trianglesDrawn", trianglesDrawn)
        .put("verticesUploaded", verticesUploaded)
        .put("modelVertices", modelVertices)
        .put("modelTriangles", modelTriangles)
        .put("submeshes", submeshes)
        .put("joints", joints)
        .put("glbImportMs", finite(glbImportMs))
        .put("initialPoseMs", finite(initialPoseMs))
        .put("rendererInitMs", finite(rendererInitMs))
        .put("nativeCreateMs", finite(nativeCreateMs))
        .put("firstFrameMs", finite(firstFrameMs))
        .put("textureBytesRequested", textureBytes)
        .put("geometryBytesRequested", geometryBytes)
        .put("renderTargetBytesRequested", renderTargetBytes)
        .put("msaaSamples", msaaSamples)
        .put("shadowMapSize", shadowMapSize)
        .put("bloomLevels", bloomLevels)
        .put("qualityLevel", qualityLevel)
        .put("glErrorFrames", glErrorFrames)
        .put("lastGlError", "0x" + lastGlError.toString(16))
        .put("eglErrorFrames", eglErrorFrames)
        .put("failedDrawCalls", failedDrawCalls)
        .put("errorCount", errorCount)
        .put("cameraYawDeg", finite(cameraYawDeg))
        .put("cameraPitchDeg", finite(cameraPitchDeg))
        .put("cameraDistanceM", finite(cameraDistanceM))
        .put("nativeHeapBytes", nativeHeapBytes)
        .put("javaHeapBytes", javaHeapBytes)
        .put("nativeSessionsCreated", nativeSessionsCreated)
        .put("nativeSessionsDestroyed", nativeSessionsDestroyed)

    /** Human-readable lines for the on-screen diagnostics panel. */
    fun summaryLines(): List<String> {
        val frameLine = if (frameSamples == 0) {
            "Frame rate: not measured yet ($framesPresented presented)"
        } else {
            "Frame rate: ${fmt(fps, 1)} FPS over the last $frameSamples frames · interval p50 ${fmt(frameIntervalP50Ms, 1)} / " +
                "p95 ${fmt(frameIntervalP95Ms, 1)} / p99 ${fmt(frameIntervalP99Ms, 1)} / max ${fmt(frameIntervalMaxMs, 1)} ms"
        }
        return listOf(
            "$backend · $apiVersion · $deviceName · ${surfaceWidth}x$surfaceHeight",
            frameLine,
            "Render call: p50 ${fmt(renderCallP50Ms, 1)} / p95 ${fmt(renderCallP95Ms, 1)} / max ${fmt(renderCallMaxMs, 1)} ms · " +
                "last frame CPU ${fmt(lastCpuMs, 1)} ms + swap ${fmt(lastSwapMs, 1)} ms",
            "Frames: $framesPresented presented · $framesNotPresented not presented · " +
                "$framesWithoutValidContent without valid content · $stallEvents stall(s)",
            "Scene: $drawCalls draw calls · $trianglesDrawn triangles drawn · model $modelTriangles triangles / " +
                "$modelVertices vertices · $submeshes submeshes · $joints joints",
            "Load: GLB import ${duration(glbImportMs)} · renderer init ${duration(rendererInitMs)} · " +
                "first frame ${duration(firstFrameMs)} · ${assetBytes / 1024L} KiB file",
            "GPU bytes requested: textures ${mib(textureBytes)} · geometry ${mib(geometryBytes)} · " +
                "render targets ${mib(renderTargetBytes)} (MSAA ${msaaSamples}x, shadow $shadowMapSize, bloom $bloomLevels, tier $qualityLevel)",
            "Process memory: native heap ${mib(nativeHeapBytes)} · Java heap ${mib(javaHeapBytes)}",
            "Errors: $glErrorFrames GL-error frame(s) (last 0x${lastGlError.toString(16)}) · " +
                "$eglErrorFrames EGL swap failure(s) · $failedDrawCalls failed draw(s)",
            "Camera: yaw ${fmt(cameraYawDeg, 1)}° · pitch ${fmt(cameraPitchDeg, 1)}° · distance ${fmt(cameraDistanceM, 3)} m",
            "Native sessions: $nativeSessionsCreated created · $nativeSessionsDestroyed destroyed"
        )
    }

    companion object {
        const val SCHEMA = "com.davemega.hub.renderer-telemetry.v1"

        private fun finite(value: Double): Double = if (value.isNaN() || value.isInfinite()) -1.0 else value
        private fun fmt(value: Double, decimals: Int): String = String.format(Locale.US, "%.${decimals}f", value)
        private fun duration(milliseconds: Double): String =
            if (milliseconds < 0.0) "not measured" else "${fmt(milliseconds, 1)} ms"
        private fun mib(bytes: Long): String = "${fmt(bytes / (1024.0 * 1024.0), 1)} MiB"
    }
}

/** Process-wide native renderer session accounting, readable with no live renderer. */
object AvatarForgeRendererRuntime {
    init {
        System.loadLibrary("mega_hub_avatarforge")
    }

    data class SessionCounters(val created: Long, val destroyed: Long) {
        /** Native sessions (renderer + engine + ANativeWindow) that have not been released. */
        val live: Long get() = created - destroyed
    }

    private external fun nativeSessionCounters(): LongArray

    fun sessionCounters(): SessionCounters {
        val values = nativeSessionCounters()
        check(values.size == 2) { "AvatarForge returned ${values.size} session counters; expected 2." }
        return SessionCounters(values[0], values[1])
    }
}
