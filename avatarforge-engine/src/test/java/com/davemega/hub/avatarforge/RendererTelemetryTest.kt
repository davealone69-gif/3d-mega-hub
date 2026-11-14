package com.davemega.hub.avatarforge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RendererTelemetryTest {
    private fun ms(value: Long): Long = value * 1_000_000L

    @Test
    fun emptyWindowReportsNothingMeasuredInsteadOfAFastFrame() {
        val window = FrameSampleWindow(8)
        assertEquals(0, window.count())
        assertEquals(0.0, window.percentileMs(50.0), 0.0)
        assertEquals(0.0, window.maxMs(), 0.0)
        assertEquals(0.0, window.ratePerSecond(), 0.0)
    }

    @Test
    fun percentilesUseNearestRankOverTheMeasuredSamples() {
        val window = FrameSampleWindow(8)
        listOf(40L, 10L, 30L, 20L).forEach { window.add(ms(it)) }
        assertEquals(4, window.count())
        assertEquals(20.0, window.percentileMs(50.0), 1e-9)
        assertEquals(40.0, window.percentileMs(95.0), 1e-9)
        assertEquals(40.0, window.percentileMs(99.0), 1e-9)
        assertEquals(40.0, window.maxMs(), 1e-9)
        // Four frames in 100 ms of accumulated frame time is 40 frames per second.
        assertEquals(40.0, window.ratePerSecond(), 1e-9)
    }

    @Test
    fun slowFramesAreNotHiddenByAnyClamp() {
        val window = FrameSampleWindow(4)
        repeat(3) { window.add(ms(8_000L)) }
        assertEquals(8_000.0, window.percentileMs(50.0), 1e-9)
        assertEquals(0.125, window.ratePerSecond(), 1e-9)
    }

    @Test
    fun windowKeepsOnlyTheMostRecentSamples() {
        val window = FrameSampleWindow(3)
        listOf(1_000L, 10L, 20L, 30L).forEach { window.add(ms(it)) }
        assertEquals(3, window.count())
        assertEquals(30.0, window.maxMs(), 1e-9)
        assertEquals(ms(60L), window.sumNs())
    }

    @Test
    fun jsonKeepsUnmeasuredDurationsNegativeAndNeverEmitsNonFiniteNumbers() {
        val telemetry = RendererTelemetry(
            backend = "OpenGL ES", apiVersion = "OpenGL ES 3.2", deviceName = "test", assetName = "a.glb",
            assetBytes = 2048L, surfaceWidth = 1080, surfaceHeight = 1102, uptimeMs = 1500L,
            framesRequested = 3L, framesPresented = 2L, framesNotPresented = 1L, framesWithoutValidContent = 1L,
            stallEvents = 0, frameSamples = 1, fps = Double.NaN,
            frameIntervalP50Ms = 16.7, frameIntervalP95Ms = 16.7, frameIntervalP99Ms = 16.7,
            frameIntervalMaxMs = Double.POSITIVE_INFINITY,
            renderCallP50Ms = 4.0, renderCallP95Ms = 5.0, renderCallMaxMs = 6.0, lastCpuMs = 3.0, lastSwapMs = 1.0,
            drawCalls = 9, trianglesDrawn = 5520, verticesUploaded = 3014, modelVertices = 3014, modelTriangles = 5520,
            submeshes = 1, joints = 0, glbImportMs = 12.5, initialPoseMs = -1.0, rendererInitMs = 80.0,
            nativeCreateMs = 95.0, firstFrameMs = -1.0, textureBytes = 20L, geometryBytes = 400_000L,
            renderTargetBytes = 90_000_000L, msaaSamples = 4, shadowMapSize = 2048, bloomLevels = 4, qualityLevel = 2,
            glErrorFrames = 1, lastGlError = 0x502, eglErrorFrames = 2, failedDrawCalls = 3,
            cameraYawDeg = 180.0, cameraPitchDeg = 4.0, cameraDistanceM = 3.25,
            nativeHeapBytes = 1L, javaHeapBytes = 2L, nativeSessionsCreated = 1L, nativeSessionsDestroyed = 0L
        )
        val json = telemetry.toJson()
        assertEquals(RendererTelemetry.SCHEMA, json.getString("schema"))
        assertEquals(-1.0, json.getDouble("fps"), 0.0)
        assertEquals(-1.0, json.getDouble("frameIntervalMaxMs"), 0.0)
        assertEquals(-1.0, json.getDouble("initialPoseMs"), 0.0)
        assertEquals(-1.0, json.getDouble("firstFrameMs"), 0.0)
        assertEquals("0x502", json.getString("lastGlError"))
        assertEquals(6, json.getInt("errorCount"))
        assertEquals(6, telemetry.errorCount)
        val lines = telemetry.summaryLines()
        assertTrue(lines.any { it.contains("first frame not measured") })
        assertTrue(lines.any { it.contains("1 GL-error frame(s) (last 0x502)") })
    }
}
