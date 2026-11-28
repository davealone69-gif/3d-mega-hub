package com.davemega.hub.avatarforge.animation

import kotlin.math.abs
import kotlin.math.exp
import kotlin.random.Random

/**
 * The face: expressions, blinking and the mouth shapes of speech, mixed into the weights of the
 * 52 face channels the renderer takes.
 *
 * Pure Kotlin with no Android dependency. Channels are addressed by their ARKit names; the order
 * the renderer wants is whatever it reports, so nothing here depends on an index.
 */
object FaceChannels {
    /** The 52 ARKit channel names, in the engine's order (kBlendshapeNames in af_rig.cpp). */
    val ARKIT: List<String> = listOf(
        "browDownLeft", "browDownRight", "browInnerUp", "browOuterUpLeft", "browOuterUpRight",
        "cheekPuff", "cheekSquintLeft", "cheekSquintRight",
        "eyeBlinkLeft", "eyeBlinkRight", "eyeLookDownLeft", "eyeLookDownRight",
        "eyeLookInLeft", "eyeLookInRight", "eyeLookOutLeft", "eyeLookOutRight",
        "eyeLookUpLeft", "eyeLookUpRight", "eyeSquintLeft", "eyeSquintRight",
        "eyeWideLeft", "eyeWideRight",
        "jawForward", "jawLeft", "jawOpen", "jawRight",
        "mouthClose", "mouthDimpleLeft", "mouthDimpleRight", "mouthFrownLeft", "mouthFrownRight",
        "mouthFunnel", "mouthLeft", "mouthLowerDownLeft", "mouthLowerDownRight",
        "mouthPressLeft", "mouthPressRight", "mouthPucker", "mouthRight",
        "mouthRollLower", "mouthRollUpper", "mouthShrugLower", "mouthShrugUpper",
        "mouthSmileLeft", "mouthSmileRight", "mouthStretchLeft", "mouthStretchRight",
        "mouthUpperUpLeft", "mouthUpperUpRight", "noseSneerLeft", "noseSneerRight",
        "tongueOut"
    )

    /** Channels of the mouth and jaw: speech takes these over from an expression while it runs. */
    fun isMouth(name: String): Boolean = name.startsWith("mouth") || name.startsWith("jaw") || name == "tongueOut"
}

/** A named face: weights (0..1) for some channels, everything else at rest. */
data class ExpressionPreset(val name: String, val label: String, val weights: Map<String, Float>)

object ExpressionPresets {
    /** Both sides of a channel that has a Left and a Right. */
    private fun both(channel: String, weight: Float) = listOf("${channel}Left" to weight, "${channel}Right" to weight)

    /** [parts] are single channels ("jawOpen" to 0.4f) and the lists [both] makes. */
    private fun preset(name: String, label: String, vararg parts: Any): ExpressionPreset {
        val weights = LinkedHashMap<String, Float>()
        fun add(entry: Any?) {
            val pair = entry as? Pair<*, *>
            val channel = pair?.first as? String
            val weight = pair?.second as? Float
            require(channel != null && weight != null) { "Expression $name: $entry is not a channel and a weight" }
            weights[channel] = weight
        }
        for (part in parts) {
            if (part is List<*>) part.forEach { add(it) } else add(part)
        }
        return ExpressionPreset(name, label, weights)
    }

    val NEUTRAL = ExpressionPreset("neutral", "Neutral", emptyMap())
    val SMILE = preset("smile", "Smile", both("mouthSmile", 0.75f), both("cheekSquint", 0.35f), both("eyeSquint", 0.15f), both("mouthDimple", 0.2f))
    val JOY = preset(
        "joy", "Laugh", both("mouthSmile", 1.0f), "jawOpen" to 0.4f, both("cheekSquint", 0.7f), both("eyeSquint", 0.45f),
        both("mouthUpperUp", 0.3f), "browInnerUp" to 0.15f
    )
    val SAD = preset(
        "sad", "Sad", both("mouthFrown", 0.75f), "browInnerUp" to 0.85f, both("eyeSquint", 0.15f), "mouthShrugLower" to 0.35f,
        both("eyeLookDown", 0.3f)
    )
    val ANGRY = preset(
        "angry", "Angry", both("browDown", 0.95f), both("eyeSquint", 0.5f), both("noseSneer", 0.6f), both("mouthPress", 0.55f),
        both("mouthFrown", 0.3f), "jawForward" to 0.2f
    )
    val SURPRISED = preset(
        "surprised", "Surprised", "browInnerUp" to 0.9f, both("browOuterUp", 0.9f), both("eyeWide", 0.9f), "jawOpen" to 0.5f,
        "mouthFunnel" to 0.25f
    )
    val FEAR = preset(
        "fear", "Afraid", "browInnerUp" to 1.0f, both("eyeWide", 0.8f), both("mouthStretch", 0.7f), "jawOpen" to 0.25f,
        both("browOuterUp", 0.3f)
    )
    val DISGUST = preset(
        "disgust", "Disgust", both("noseSneer", 0.9f), both("mouthUpperUp", 0.6f), both("browDown", 0.5f), both("eyeSquint", 0.4f),
        both("mouthFrown", 0.3f)
    )
    val WINK = preset("wink", "Wink", "eyeBlinkLeft" to 1.0f, "cheekSquintLeft" to 0.6f, "mouthSmileLeft" to 0.55f, "mouthSmileRight" to 0.3f)
    val KISS = preset("kiss", "Kiss", "mouthPucker" to 0.9f, "mouthFunnel" to 0.3f, both("eyeBlink", 0.35f))
    val SKEPTICAL = preset(
        "skeptical", "Doubt", "browOuterUpLeft" to 0.85f, "browDownRight" to 0.65f, "mouthLeft" to 0.4f, "mouthPressRight" to 0.35f,
        "eyeSquintRight" to 0.3f
    )

    val ALL: List<ExpressionPreset> = listOf(NEUTRAL, SMILE, JOY, SAD, ANGRY, SURPRISED, FEAR, DISGUST, WINK, KISS, SKEPTICAL)

    fun find(name: String): ExpressionPreset? = ALL.firstOrNull { it.name.equals(name, ignoreCase = true) || it.label.equals(name, ignoreCase = true) }
}

/**
 * Blinks at irregular intervals, as eyes do: closed in about 70 ms, open again in about 130 ms,
 * every two to five seconds, now and then twice in a row. The same seed gives the same blinks.
 */
class BlinkController(
    seed: Long = 1L,
    private val minInterval: Float = 2.2f,
    private val maxInterval: Float = 5.5f,
    private val closeSeconds: Float = 0.07f,
    private val holdSeconds: Float = 0.03f,
    private val openSeconds: Float = 0.13f,
    private val doubleBlinkChance: Float = 0.15f
) {
    private val random = Random(seed)
    private var untilBlink = interval()
    private var intoBlink = -1f            /* seconds into the running blink, negative when the eyes are open */

    val blinkSeconds: Float get() = closeSeconds + holdSeconds + openSeconds

    /** Blinks completed so far. */
    var blinks: Int = 0
        private set

    private fun interval(): Float = minInterval + (maxInterval - minInterval) * random.nextFloat()

    /** Moves the clock on and returns how closed the eyes are, 0..1. */
    fun advance(deltaSeconds: Float): Float {
        var dt = if (deltaSeconds.isFinite() && deltaSeconds > 0f) deltaSeconds.coerceAtMost(1f) else 0f
        while (dt > 0f) {
            if (intoBlink < 0f) {
                if (untilBlink > dt) {
                    untilBlink -= dt
                    dt = 0f
                } else {
                    dt -= untilBlink
                    intoBlink = 0f
                }
            } else {
                val left = blinkSeconds - intoBlink
                if (left > dt) {
                    intoBlink += dt
                    dt = 0f
                } else {
                    dt -= left
                    intoBlink = -1f
                    blinks++
                    untilBlink = if (random.nextFloat() < doubleBlinkChance) 0.18f else interval()
                }
            }
        }
        return closedness()
    }

    private fun closedness(): Float {
        val t = intoBlink
        if (t < 0f) return 0f
        return when {
            t < closeSeconds -> smooth(t / closeSeconds)
            t < closeSeconds + holdSeconds -> 1f
            else -> 1f - smooth((t - closeSeconds - holdSeconds) / openSeconds)
        }
    }

    private fun smooth(x: Float): Float {
        val t = x.coerceIn(0f, 1f)
        return t * t * (3f - 2f * t)
    }
}

/** A mouth shape of speech: the channels that make it. */
enum class Viseme(val weights: Map<String, Float>) {
    /** Lips together at rest: between words. */
    REST(emptyMap()),
    /** a as in "father": the jaw drops. */
    AA(mapOf("jawOpen" to 0.62f, "mouthLowerDownLeft" to 0.3f, "mouthLowerDownRight" to 0.3f)),
    /** e, i: the lips spread. */
    EE(mapOf("jawOpen" to 0.22f, "mouthStretchLeft" to 0.5f, "mouthStretchRight" to 0.5f, "mouthSmileLeft" to 0.25f, "mouthSmileRight" to 0.25f)),
    /** o: round and open. */
    OH(mapOf("jawOpen" to 0.42f, "mouthFunnel" to 0.6f, "mouthPucker" to 0.2f)),
    /** u, w: round and nearly closed. */
    OO(mapOf("jawOpen" to 0.15f, "mouthPucker" to 0.8f, "mouthFunnel" to 0.3f)),
    /** m, b, p: the lips press together. */
    MBP(mapOf("mouthPressLeft" to 0.55f, "mouthPressRight" to 0.55f, "mouthRollLower" to 0.2f, "mouthRollUpper" to 0.2f)),
    /** f, v: the lower lip under the upper teeth. */
    FV(mapOf("jawOpen" to 0.1f, "mouthRollLower" to 0.6f, "mouthUpperUpLeft" to 0.25f, "mouthUpperUpRight" to 0.25f)),
    /** l, t, d, n, th: the tongue at the teeth, the mouth a little open. */
    TD(mapOf("jawOpen" to 0.26f, "mouthLowerDownLeft" to 0.22f, "mouthLowerDownRight" to 0.22f)),
    /** s, z, sh, ch, j: teeth nearly together, lips apart. */
    SZ(mapOf("jawOpen" to 0.1f, "mouthStretchLeft" to 0.35f, "mouthStretchRight" to 0.35f, "mouthUpperUpLeft" to 0.15f, "mouthUpperUpRight" to 0.15f)),
    /** k, g, h, r and the rest: made in the throat, the mouth just open. */
    KG(mapOf("jawOpen" to 0.3f, "mouthStretchLeft" to 0.1f, "mouthStretchRight" to 0.1f));

    companion object {
        /** The shape of one letter; null for anything that is not spoken (space, punctuation). */
        fun ofLetter(letter: Char): Viseme? = when (letter.lowercaseChar()) {
            'a', 'à', 'á', 'â', 'ä', 'å', 'ã' -> AA
            'e', 'i', 'y', 'è', 'é', 'ê', 'ë', 'ì', 'í', 'î', 'ï' -> EE
            'o', 'ò', 'ó', 'ô', 'ö', 'õ' -> OH
            'u', 'w', 'ù', 'ú', 'û', 'ü' -> OO
            'm', 'b', 'p' -> MBP
            'f', 'v' -> FV
            'l', 't', 'd', 'n' -> TD
            's', 'z', 'c', 'j', 'x' -> SZ
            'k', 'g', 'h', 'r', 'q' -> KG
            else -> if (letter.isLetter()) KG else null
        }
    }
}

/**
 * The mouth shapes of a text over time, worked out from its letters.
 *
 * It is an estimate, not a recording: the speech engine gives no phonemes. Each letter gets a
 * shape and a share of time, vowels a little longer than consonants, with a pause at a space and
 * a longer one at punctuation. [timeOfChar] lets a caller pull the clock back in line whenever
 * the speech engine says which word it has reached.
 */
class LipSyncTimeline private constructor(
    val text: String,
    private val keys: List<Key>,
    val durationSeconds: Float
) {
    data class Key(val timeSeconds: Float, val viseme: Viseme, val charIndex: Int)

    val keyCount: Int get() = keys.size
    fun key(index: Int): Key = keys[index]

    /** The time at which the character at [index] of the text begins. */
    fun timeOfChar(index: Int): Float {
        if (keys.isEmpty()) return 0f
        if (index <= 0) return 0f
        var best = keys.last().timeSeconds
        for (key in keys) {
            if (key.charIndex >= index) {
                best = key.timeSeconds
                break
            }
        }
        return best
    }

    /** The shape at [timeSeconds], blended between the letters either side of it, written into [into]. */
    fun weightsAt(timeSeconds: Float, into: MutableMap<String, Float>) {
        into.clear()
        if (keys.isEmpty() || !timeSeconds.isFinite() || timeSeconds < 0f || timeSeconds >= durationSeconds) return
        var next = keys.indexOfFirst { it.timeSeconds > timeSeconds }
        if (next < 0) next = keys.size
        val from = if (next > 0) keys[next - 1] else null
        val to = if (next < keys.size) keys[next] else null
        val fromTime = from?.timeSeconds ?: 0f
        val toTime = to?.timeSeconds ?: durationSeconds
        val span = toTime - fromTime
        val x = if (span > 1e-6f) ((timeSeconds - fromTime) / span).coerceIn(0f, 1f) else 1f
        val s = x * x * (3f - 2f * x)
        from?.viseme?.weights?.forEach { (name, weight) -> into[name] = weight * (1f - s) }
        to?.viseme?.weights?.forEach { (name, weight) -> into[name] = (into[name] ?: 0f) + weight * s }
    }

    companion object {
        /** About the rate of Android's speech engines at their normal speed. */
        const val DEFAULT_CHARS_PER_SECOND = 14f

        fun fromText(text: String, charsPerSecond: Float = DEFAULT_CHARS_PER_SECOND): LipSyncTimeline {
            val rate = if (charsPerSecond.isFinite() && charsPerSecond > 1f) charsPerSecond else DEFAULT_CHARS_PER_SECOND
            val unit = 1f / rate
            val keys = ArrayList<Key>()
            var time = 0f
            var last: Viseme? = null
            for ((index, c) in text.withIndex()) {
                val viseme = Viseme.ofLetter(c)
                if (viseme != null) {
                    /* A doubled shape ("ll", "ee") is one shape held, not two. */
                    if (viseme != last) keys += Key(time, viseme, index)
                    time += unit * (if (viseme == Viseme.AA || viseme == Viseme.EE || viseme == Viseme.OH || viseme == Viseme.OO) 1.25f else 0.85f)
                    last = viseme
                } else if (c.isDigit()) {
                    /* A digit is read as a word: open and spread, about three letters long. */
                    keys += Key(time, Viseme.AA, index)
                    keys += Key(time + unit * 1.5f, Viseme.EE, index)
                    time += unit * 3f
                    last = Viseme.EE
                } else {
                    val pause = when (c) {
                        '.', '!', '?', ';', ':' -> 5f
                        ',', '\n', '(', ')', '-' -> 2.5f
                        else -> 0.6f
                    }
                    if (last != Viseme.REST && keys.isNotEmpty()) keys += Key(time, Viseme.REST, index)
                    time += unit * pause
                    last = Viseme.REST
                }
            }
            if (keys.isNotEmpty() && keys.last().viseme != Viseme.REST) keys += Key(time, Viseme.REST, text.length)
            return LipSyncTimeline(text, keys, if (keys.isEmpty()) 0f else time + unit)
        }
    }
}

/**
 * Mixes an expression, the blink, single channels set by hand and the mouth of speech into the
 * weights the renderer takes, once a frame. Thread-safe: a screen sets things from the main
 * thread and the render thread calls [tick].
 *
 * While speech runs the mouth belongs to it: the expression's mouth channels are turned down to
 * make room, the rest of the face keeps the expression. Changes are eased over a few frames so a
 * new expression does not pop; the blink is not eased, its shape is already its own.
 */
class FaceAnimator(channelNames: List<String>, seed: Long = 1L) {
    private val names: List<String> = channelNames.toList()
    private val indexOf: Map<String, Int> = names.withIndex().associate { it.value to it.index }
    private val mouth = BooleanArray(names.size) { FaceChannels.isMouth(names[it]) }
    private val blinkLeft = indexOf["eyeBlinkLeft"] ?: -1
    private val blinkRight = indexOf["eyeBlinkRight"] ?: -1
    private val wideLeft = indexOf["eyeWideLeft"] ?: -1
    private val wideRight = indexOf["eyeWideRight"] ?: -1

    private val blink = BlinkController(seed)
    private val target = FloatArray(names.size)
    private val eased = FloatArray(names.size)
    private val output = FloatArray(names.size)
    private val sent = FloatArray(names.size)
    private var everSent = false
    private val speechWeights = HashMap<String, Float>()

    private var expression: ExpressionPreset = ExpressionPresets.NEUTRAL
    private var intensity = 1f
    private val manual = HashMap<String, Float>()
    private var blinking = true
    private var timeline: LipSyncTimeline? = null
    private var speechTime = 0f

    val channelCount: Int get() = names.size

    @Synchronized fun setExpression(preset: ExpressionPreset?, strength: Float = 1f) {
        expression = preset ?: ExpressionPresets.NEUTRAL
        intensity = if (strength.isFinite()) strength.coerceIn(0f, 1f) else 1f
    }

    /** One channel by hand, over the expression; a weight of 0 or less hands it back. */
    @Synchronized fun setChannel(name: String, weight: Float) {
        if (!weight.isFinite() || weight <= 0f) manual.remove(name) else manual[name] = weight.coerceIn(0f, 1f)
    }

    @Synchronized fun clearChannels() = manual.clear()

    @Synchronized fun setBlinking(enabled: Boolean) {
        blinking = enabled
    }

    /** Starts the mouth on [speech] from its beginning. */
    @Synchronized fun speak(speech: LipSyncTimeline) {
        timeline = speech
        speechTime = 0f
    }

    /** The speech engine has reached the character at [charIndex]: put the mouth there. */
    @Synchronized fun syncSpeechToChar(charIndex: Int) {
        val running = timeline ?: return
        val reached = running.timeOfChar(charIndex)
        /* Only pull the clock when it has drifted by more than a letter or two, so a callback
           that arrives a frame late does not make the mouth stutter. */
        if (abs(reached - speechTime) > 0.12f) speechTime = reached
    }

    @Synchronized fun stopSpeech() {
        timeline = null
        speechTime = 0f
    }

    val speaking: Boolean
        @Synchronized get() = timeline != null

    /** The weight last computed for [name]; 0 for a channel the model does not have. */
    @Synchronized fun weightOf(name: String): Float = indexOf[name]?.let { output[it] } ?: 0f

    /**
     * Moves every clock on by [deltaSeconds] and returns the weights, in the renderer's channel
     * order - or null when they are what the last call returned, so there is nothing to send.
     * The array is reused by the next call.
     */
    @Synchronized fun tick(deltaSeconds: Float): FloatArray? {
        val dt = if (deltaSeconds.isFinite() && deltaSeconds > 0f) deltaSeconds.coerceAtMost(0.25f) else 0f

        var speech = 0f
        val running = timeline
        if (running != null) {
            speechTime += dt
            if (speechTime >= running.durationSeconds) {
                timeline = null
                speechTime = 0f
                speechWeights.clear()
            } else {
                running.weightsAt(speechTime, speechWeights)
                speech = 1f
            }
        } else {
            speechWeights.clear()
        }

        java.util.Arrays.fill(target, 0f)
        for ((name, weight) in expression.weights) {
            val i = indexOf[name] ?: continue
            target[i] = weight * intensity * (if (mouth[i]) 1f - 0.75f * speech else 1f)
        }
        for ((name, weight) in speechWeights) {
            val i = indexOf[name] ?: continue
            target[i] = (target[i] + weight).coerceAtMost(1f)
        }
        for ((name, weight) in manual) {
            val i = indexOf[name] ?: continue
            target[i] = weight
        }

        /* Ease towards the target: 63 % of the way in 60 ms. */
        val blend = if (dt > 0f) 1f - exp(-dt / 0.06f) else 0f
        for (i in eased.indices) {
            eased[i] += (target[i] - eased[i]) * blend
            if (abs(target[i] - eased[i]) < 0.0005f) eased[i] = target[i]
            output[i] = eased[i]
        }

        val closed = if (blinking) blink.advance(dt) else 0f
        if (closed > 0f) {
            if (blinkLeft >= 0) output[blinkLeft] = maxOf(output[blinkLeft], closed)
            if (blinkRight >= 0) output[blinkRight] = maxOf(output[blinkRight], closed)
            if (wideLeft >= 0) output[wideLeft] *= 1f - closed
            if (wideRight >= 0) output[wideRight] *= 1f - closed
        }

        var changed = !everSent
        for (i in output.indices) {
            if (abs(output[i] - sent[i]) > 0.0005f) changed = true
        }
        if (!changed) return null
        System.arraycopy(output, 0, sent, 0, output.size)
        everSent = true
        return output
    }
}
