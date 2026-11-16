package com.davemega.hub

/**
 * Speech output settings for the Voice page.
 *
 * The page previously spoke every utterance at the engine's defaults with no way to pick a voice,
 * so a device whose default voice is the wrong language or speed had no remedy inside the app. The
 * ranges here mirror what `android.speech.tts.TextToSpeech` documents for `setSpeechRate` and
 * `setPitch` (0.25x–4x); values outside them are clamped rather than passed through, because the
 * engine throws or silently misbehaves on out-of-contract input.
 *
 * `voiceName` is the engine's own voice name (`Voice.getName()`). `null` means "engine default",
 * and a saved name that the engine no longer reports falls back to the default rather than failing.
 */
data class SpeechSettings(
    val rate: Float = 1.0f,
    val pitch: Float = 1.0f,
    val voiceName: String? = null
) {
    fun normalized(): SpeechSettings = copy(
        rate = rate.finiteOr(1.0f).coerceIn(MIN_RATE, MAX_RATE),
        pitch = pitch.finiteOr(1.0f).coerceIn(MIN_PITCH, MAX_PITCH),
        voiceName = voiceName?.trim()?.takeIf { it.isNotEmpty() }
    )

    /** One line for the Voice page, e.g. `1.25x rate · pitch 0.90 · voice en-gb-x-gba-local`. */
    fun describe(): String {
        val clean = normalized()
        val voice = clean.voiceName ?: "engine default"
        return "${format(clean.rate)}x rate · pitch ${format(clean.pitch)} · $voice"
    }

    companion object {
        const val MIN_RATE = 0.25f
        const val MAX_RATE = 4.0f
        const val MIN_PITCH = 0.25f
        const val MAX_PITCH = 4.0f

        /** `rate|pitch|voice`, tolerant of a missing or mangled third field. */
        fun encode(settings: SpeechSettings): String {
            val clean = settings.normalized()
            return "${clean.rate}|${clean.pitch}|${clean.voiceName.orEmpty()}"
        }

        fun decode(raw: String?): SpeechSettings {
            if (raw.isNullOrBlank()) return SpeechSettings()
            val parts = raw.split('|')
            return SpeechSettings(
                rate = parts.getOrNull(0)?.toFloatOrNull() ?: 1.0f,
                pitch = parts.getOrNull(1)?.toFloatOrNull() ?: 1.0f,
                voiceName = parts.getOrNull(2)?.trim()?.takeIf { it.isNotEmpty() }
            ).normalized()
        }
    }
}

/** Two decimal places without `String.format`, so this stays locale-independent and JVM-testable. */
internal fun format(value: Float): String {
    val scaled = Math.round(value * 100.0f)
    val whole = scaled / 100
    val fraction = Math.abs(scaled % 100)
    return "$whole.${fraction.toString().padStart(2, '0')}"
}

/**
 * The voice name to actually install on the engine: the requested one if the engine still reports
 * it, otherwise `null` (engine default).
 */
object SpeechVoices {
    fun resolve(available: List<String>, wanted: String?): String? {
        val clean = wanted?.trim()?.takeIf { it.isNotEmpty() } ?: return null
        return available.firstOrNull { it == clean }
    }

    /**
     * Next voice name in the engine's list, wrapping around and including "engine default" (`null`)
     * as the first entry of the cycle.
     */
    fun cycle(available: List<String>, current: String?): String? {
        val options: List<String?> = listOf(null) + available.distinct()
        if (options.size <= 1) return null
        /* An unknown saved voice behaves like "engine default": the cycle restarts at the first real
           option rather than at `null`, which a caller has just come from. */
        val index = options.indexOfFirst { it == current }.coerceAtLeast(0)
        return options[(index + 1) % options.size]
    }

    /** Short label for the cycle button. */
    fun label(current: String?, available: List<String>): String {
        val resolved = resolve(available, current)
        if (resolved != null) return resolved
        return if (available.isEmpty()) "No voices reported" else "Engine default"
    }
}
