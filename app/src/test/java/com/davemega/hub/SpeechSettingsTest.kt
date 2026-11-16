package com.davemega.hub

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class SpeechSettingsTest {
    @Test
    fun defaultsAreEngineDefaults() {
        val settings = SpeechSettings().normalized()

        assertEquals(1.0f, settings.rate, 0.0001f)
        assertEquals(1.0f, settings.pitch, 0.0001f)
        assertNull(settings.voiceName)
    }

    @Test
    fun rateAndPitchAreClampedToTheDocumentedRange() {
        val high = SpeechSettings(rate = 12f, pitch = 9f).normalized()
        assertEquals(SpeechSettings.MAX_RATE, high.rate, 0.0001f)
        assertEquals(SpeechSettings.MAX_PITCH, high.pitch, 0.0001f)

        val low = SpeechSettings(rate = 0.01f, pitch = -3f).normalized()
        assertEquals(SpeechSettings.MIN_RATE, low.rate, 0.0001f)
        assertEquals(SpeechSettings.MIN_PITCH, low.pitch, 0.0001f)
    }

    @Test
    fun nanRateIsRepairedToTheDefaultNotToAClampEdge() {
        val repaired = SpeechSettings(rate = Float.NaN, pitch = Float.POSITIVE_INFINITY).normalized()

        assertEquals(1.0f, repaired.rate, 0.0001f)
        assertEquals(1.0f, repaired.pitch, 0.0001f)
    }

    @Test
    fun blankVoiceNameMeansDefault() {
        assertNull(SpeechSettings(voiceName = "   ").normalized().voiceName)
        assertEquals("en-gb-x-gba-local", SpeechSettings(voiceName = " en-gb-x-gba-local ").normalized().voiceName)
    }

    @Test
    fun persistenceRoundTrips() {
        val original = SpeechSettings(rate = 1.25f, pitch = 0.9f, voiceName = "en-us-x-sfg#female_1-local")

        val restored = SpeechSettings.decode(SpeechSettings.encode(original))

        assertEquals(1.25f, restored.rate, 0.0001f)
        assertEquals(0.9f, restored.pitch, 0.0001f)
        assertEquals("en-us-x-sfg#female_1-local", restored.voiceName)
    }

    @Test
    fun garbagePersistenceFallsBackToDefaults() {
        assertEquals(SpeechSettings(), SpeechSettings.decode(null))
        assertEquals(SpeechSettings(), SpeechSettings.decode(""))
        assertEquals(SpeechSettings(), SpeechSettings.decode("not-a-number|also-not|||"))
        assertEquals(SpeechSettings(), SpeechSettings.decode("||"))
    }

    @Test
    fun describeShowsRatePitchAndVoice() {
        assertEquals("1.00x rate · pitch 1.00 · engine default", SpeechSettings().describe())
        assertEquals(
            "1.25x rate · pitch 0.90 · en-gb-x-gba-local",
            SpeechSettings(rate = 1.25f, pitch = 0.9f, voiceName = "en-gb-x-gba-local").describe()
        )
    }

    @Test
    fun resolveOnlyAcceptsAVoiceTheEngineStillReports() {
        val available = listOf("a", "b")
        assertEquals("b", SpeechVoices.resolve(available, "b"))
        assertNull(SpeechVoices.resolve(available, "gone"))
        assertNull(SpeechVoices.resolve(available, null))
        assertNull(SpeechVoices.resolve(emptyList(), "a"))
    }

    @Test
    fun cycleWalksDefaultThenEveryVoiceAndWraps() {
        val available = listOf("a", "b")

        assertEquals("a", SpeechVoices.cycle(available, null))
        assertEquals("b", SpeechVoices.cycle(available, "a"))
        assertNull(SpeechVoices.cycle(available, "b"))
        assertNull(SpeechVoices.cycle(emptyList(), null))
        /* An unknown saved voice restarts the cycle at the first real option. */
        assertEquals("a", SpeechVoices.cycle(available, "gone"))
    }

    @Test
    fun labelNeverShowsAStaleVoiceName() {
        assertTrue(SpeechVoices.label("gone", listOf("a")).startsWith("Engine default"))
        assertEquals("No voices reported", SpeechVoices.label(null, emptyList()))
        assertEquals("a", SpeechVoices.label("a", listOf("a")))
    }
}
