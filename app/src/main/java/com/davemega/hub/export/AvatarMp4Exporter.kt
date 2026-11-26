package com.davemega.hub.export

import android.graphics.Bitmap
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaExtractor
import android.media.MediaFormat
import android.media.MediaMetadataRetriever
import android.media.MediaMuxer
import android.os.SystemClock
import android.util.Log
import android.view.Surface
import com.davemega.hub.FilamentPreviewView
import java.io.File
import kotlin.math.PI
import kotlin.math.min

/**
 * Encodes an MP4 of a model already loaded into a [FilamentPreviewView], by rendering that view's OWN
 * Engine, View, Camera and gltfio Animator into a hardware encoder's input Surface. No second
 * renderer is created beside the preview, which was the explicit constraint for porting 3DDD's
 * export rather than transplanting it.
 *
 * WHERE THIS CAME FROM AND WHAT CHANGED
 * -------------------------------------
 * The codec configuration, the per-frame output drain, the end-of-stream drain with a deadline and
 * the container validation are adapted from `ExportManager.exportMp4` in `davealone69-gif/3DDD` at
 * `f70e9f43`. That code had three defects this port does not carry:
 *
 *  1. It set `renderer.orbit.azimuth` each frame but never called `OrbitCamera.apply()`, and apply()
 *     is what moves the camera, so every exported frame showed one viewpoint. Here the view calls
 *     `lookAt()` on the very frame it renders (see FilamentPreviewView.renderEncoderFrame).
 *
 *  2. Its encode loop never advanced the character's Animator, so the file was a static model. Here
 *     each frame is sampled at exactly its index/fps seconds of the clip; encoding speed cannot
 *     change the motion because no wall clock is involved.
 *
 *  3. Its test checked that the MP4 finalises and plays, but never that two frames differ or that
 *     the model animates. [measureFrameDifference] decodes two real frames out of the finished file
 *     and reports how many pixels changed, so a caller can assert visible motion instead of trusting
 *     a container that merely parses. A turntable can also be switched off, after which any
 *     difference between frames has to come from the character rather than the camera.
 *
 * WHAT THE NUMBERS MEAN
 * ---------------------
 * Everything reported in [Mp4Export] is measured from the finished file or counted as it happens.
 * Anything that could not be measured is null, never 0: a retriever that will not supply a duration
 * records `durationMs = null`; two frames that will not decode record `frameDifference = null`. A 0
 * here always means "measured, and it really was zero", which for frame difference would be a finding
 * (no visible motion), not an absence of measurement.
 */
object AvatarMp4Exporter {

    private const val TAG = "AvatarMp4Exporter"
    private const val DEQUEUE_TIMEOUT_US = 10_000L
    private const val EOS_DEADLINE_MS = 30_000L

    /** One completed export. All fields are measured; see the class note on nulls vs zeros. */
    data class Mp4Export(
        val file: File,
        val framesSubmitted: Int,
        val fps: Int,
        val seconds: Int,
        val turntable: Boolean,
        val container: ContainerProbe,
        val frameDifference: FrameDifference?,
        val notes: List<String>,
    )

    /** What reading the container back reveals. Nulls are unmeasurable, never fabricated. */
    data class ContainerProbe(
        val bytes: Long,
        val durationMs: Long?,
        val trackWidth: Int?,
        val trackHeight: Int?,
        val readableSamples: Int,
    )

    /** Two decoded frames compared. differingPixels == 0 with totalPixels > 0 means the file shows
     *  no visible motion between those times -- a real result, distinct from a failed decode (null). */
    data class FrameDifference(
        val firstTimeUs: Long,
        val secondTimeUs: Long,
        val differingPixels: Int,
        val totalPixels: Int,
        val maxChannelDelta: Int,
    ) {
        val fractionDiffering: Double
            get() = if (totalPixels <= 0) 0.0 else differingPixels.toDouble() / totalPixels.toDouble()
    }

    /**
     * Renders [seconds] of the currently selected clip at [fps] into [file]. Blocks until done; run
     * it off the main thread (it renders and encodes for the whole duration). Progress reports land
     * in the range 0..1 and do not reach 1 until validation has passed.
     */
    fun exportMp4(
        view: FilamentPreviewView,
        file: File,
        width: Int = 1080,
        height: Int = 1920,
        fps: Int = 30,
        seconds: Int = 8,
        turntable: Boolean = true,
        onProgress: (Float) -> Unit = {},
    ): Mp4Export {
        require(width >= 16 && height >= 16 && width % 2 == 0 && height % 2 == 0) {
            "MP4 dimensions must be even and at least 16 pixels, got ${width}x$height"
        }
        require(fps in 1..60) { "MP4 frame rate must be between 1 and 60 fps, got $fps" }
        require(seconds in 1..120) { "MP4 duration must be between 1 and 120 seconds, got $seconds" }
        val frameCount = Math.multiplyExact(fps, seconds)

        file.parentFile?.mkdirs()
        if (file.exists() && !file.delete()) throw IllegalStateException("could not replace ${file.absolutePath}")

        var muxer: MediaMuxer? = null
        var codec: MediaCodec? = null
        var swapChain: com.google.android.filament.SwapChain? = null
        var encoderSurface: Surface? = null
        var muxerStarted = false
        var finalized = false
        val previewWasRunning = view.suspendPreviewForExport()
        val notes = mutableListOf<String>()
        var framesSubmitted = 0
        try {
            val activeMuxer = MediaMuxer(file.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
            muxer = activeMuxer

            val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height).apply {
                setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
                setInteger(MediaFormat.KEY_BIT_RATE, bitrateFor(width, height, fps))
                setInteger(MediaFormat.KEY_FRAME_RATE, fps)
                setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
                setInteger(MediaFormat.KEY_BITRATE_MODE, MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR)
            }
            val activeCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
            codec = activeCodec
            activeCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            val surface = activeCodec.createInputSurface()
            encoderSurface = surface
            activeCodec.start()

            val activeSwapChain = view.createEncoderSwapChain(surface)
                ?: throw IllegalStateException("the preview view created no encoder SwapChain; the engine may not exist yet")
            swapChain = activeSwapChain

            val clipDuration = view.selectedClipDurationSeconds()
            if (clipDuration == null) {
                /* Honest rather than silent: the model has no clip, so every frame is the same pose.
                   Motion in the file, if any, then comes only from the turntable. */
                notes += "no animation clip was available, so every frame shows the bind pose"
            }

            val bufferInfo = MediaCodec.BufferInfo()
            var trackIndex = -1
            val frameDurationNs = 1_000_000_000L / fps
            var outputEos = false

            for (frame in 0 until frameCount) {
                val timestampNs = frame * frameDurationNs
                /* Clip position is a function of the frame index alone, wrapped so a short clip loops
                   across a longer export instead of freezing on its last keyframe. */
                val animationSeconds = if (clipDuration != null && clipDuration > 0f) {
                    (frame.toFloat() / fps) % clipDuration
                } else {
                    0f
                }
                /* azimuth 0 is the framing the user is looking at, so a still-camera export begins
                   there too. */
                val azimuth = if (turntable) 2.0 * PI * frame.toDouble() / frameCount else 0.0

                val rendered = view.renderEncoderFrame(
                    activeSwapChain, timestampNs, animationSeconds, azimuth, width, height
                )
                if (!rendered) {
                    throw IllegalStateException(
                        "the renderer refused frame $frame of $frameCount; encoding stopped rather than " +
                            "letting the encoder repeat the previous frame and report motion that was never drawn"
                    )
                }
                framesSubmitted++
                onProgress(frame.toFloat() / frameCount * 0.9f)

                /* Drain whatever the encoder has produced so far, the same way 3DDD did. Skipping the
                   CODEC_CONFIG sample and starting the muxer on the first real one is required, not
                   optional: writing config data as media corrupts the track. */
                var index = activeCodec.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US)
                while (index >= 0) {
                    val output = activeCodec.getOutputBuffer(index)
                        ?: throw IllegalStateException("encoder returned a null output buffer")
                    if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG == 0 && bufferInfo.size > 0) {
                        if (!muxerStarted) {
                            trackIndex = activeMuxer.addTrack(activeCodec.outputFormat)
                            activeMuxer.start()
                            muxerStarted = true
                        }
                        output.position(bufferInfo.offset)
                        output.limit(bufferInfo.offset + bufferInfo.size)
                        activeMuxer.writeSampleData(trackIndex, output, bufferInfo)
                    }
                    activeCodec.releaseOutputBuffer(index, false)
                    if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) outputEos = true
                    index = activeCodec.dequeueOutputBuffer(bufferInfo, 0)
                }
                if (outputEos) {
                    throw IllegalStateException("encoder ended before all $frameCount frames were submitted")
                }
            }

            activeCodec.signalEndOfInputStream()
            var draining = true
            val deadline = SystemClock.elapsedRealtime() + EOS_DEADLINE_MS
            while (draining) {
                if (SystemClock.elapsedRealtime() > deadline) {
                    throw IllegalStateException("timed out after ${EOS_DEADLINE_MS} ms waiting for encoder end-of-stream")
                }
                val index = activeCodec.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US)
                when {
                    index == MediaCodec.INFO_TRY_AGAIN_LATER -> Unit
                    index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                        if (!muxerStarted) {
                            trackIndex = activeMuxer.addTrack(activeCodec.outputFormat)
                            activeMuxer.start()
                            muxerStarted = true
                        }
                    }
                    index >= 0 -> {
                        val output = activeCodec.getOutputBuffer(index)
                            ?: throw IllegalStateException("encoder returned a null output buffer while draining")
                        if (bufferInfo.size > 0 && muxerStarted &&
                            bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG == 0
                        ) {
                            output.position(bufferInfo.offset)
                            output.limit(bufferInfo.offset + bufferInfo.size)
                            activeMuxer.writeSampleData(trackIndex, output, bufferInfo)
                        }
                        val eos = bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0
                        activeCodec.releaseOutputBuffer(index, false)
                        if (eos) draining = false
                    }
                }
            }

            if (!muxerStarted) throw IllegalStateException("encoder produced no muxable video track")
            /* Finalisation errors escape: a half-written container is never handed back as an export. */
            activeCodec.stop()
            activeMuxer.stop()
            finalized = true
            onProgress(0.95f)

            val container = probeContainer(file)
            if (container.readableSamples < 1) {
                throw IllegalStateException("exported MP4 contains no readable samples")
            }
            /* Decode two frames well apart and compare them. A file that parses and plays but whose
               frames are identical exports as no motion at all, and that is a result worth surfacing
               even though the container itself is valid. */
            val diff = measureFrameDifference(file)
            if (diff != null && diff.totalPixels > 0 && diff.differingPixels == 0) {
                notes += "decoded frames at ${diff.firstTimeUs}us and ${diff.secondTimeUs}us were pixel-identical, so the file shows no visible motion"
            } else if (diff == null) {
                notes += "two frames could not be decoded for comparison, so visible motion is unverified"
            }
            onProgress(1f)

            return Mp4Export(
                file = file,
                framesSubmitted = framesSubmitted,
                fps = fps,
                seconds = seconds,
                turntable = turntable,
                container = container,
                frameDifference = diff,
                notes = notes.toList(),
            )
        } catch (failure: Throwable) {
            if (file.exists()) runCatching { file.delete() }
            throw failure
        } finally {
            view.resumePreviewAfterExport(previewWasRunning)
            swapChain?.let { runCatching { view.destroyEncoderSwapChain(it) } }
            encoderSurface?.let { runCatching { it.release() } }
            codec?.let {
                if (!finalized) runCatching { it.stop() }
                runCatching { it.release() }
            }
            muxer?.let {
                if (muxerStarted && !finalized) runCatching { it.stop() }
                runCatching { it.release() }
            }
        }
    }

    /** Reads the finished container back with a MediaExtractor and a MediaMetadataRetriever. Fields
     *  the platform will not provide are left null rather than guessed. */
    fun probeContainer(file: File): ContainerProbe {
        if (!file.isFile || file.length() < 32) {
            throw IllegalStateException("MP4 output is missing or too small (${file.length()} bytes) to be a real video")
        }
        var trackWidth: Int? = null
        var trackHeight: Int? = null
        var samples = 0
        val extractor = MediaExtractor()
        try {
            extractor.setDataSource(file.absolutePath)
            var videoTrack = -1
            for (i in 0 until extractor.trackCount) {
                val mime = extractor.getTrackFormat(i).getString(MediaFormat.KEY_MIME) ?: continue
                if (mime.startsWith("video/")) {
                    videoTrack = i
                    break
                }
            }
            if (videoTrack < 0) throw IllegalStateException("MP4 has no video track")
            extractor.selectTrack(videoTrack)
            val fmt = extractor.getTrackFormat(videoTrack)
            if (fmt.containsKey(MediaFormat.KEY_WIDTH)) trackWidth = fmt.getInteger(MediaFormat.KEY_WIDTH)
            if (fmt.containsKey(MediaFormat.KEY_HEIGHT)) trackHeight = fmt.getInteger(MediaFormat.KEY_HEIGHT)
            if ((trackWidth ?: 0) <= 0 || (trackHeight ?: 0) <= 0) {
                throw IllegalStateException("MP4 video track has no usable dimensions ($trackWidth x $trackHeight)")
            }
            /* Count readable samples; a track with headers but no payload is not an export. */
            extractor.seekTo(0L, MediaExtractor.SEEK_TO_CLOSEST_SYNC)
            val buffer = java.nio.ByteBuffer.allocate(256 * 1024)
            val info = MediaCodec.BufferInfo()
            while (true) {
                val n = extractor.readSampleData(buffer, 0)
                if (n < 0) break
                samples++
                extractor.advance()
            }
        } finally {
            runCatching { extractor.release() }
        }

        var durationMs: Long? = null
        val retriever = MediaMetadataRetriever()
        try {
            retriever.setDataSource(file.absolutePath)
            val raw = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull()
            if (raw != null && raw > 0L) durationMs = raw
        } catch (error: Exception) {
            Log.w(TAG, "duration metadata unavailable for ${file.name}: ${error.message}")
        } finally {
            runCatching { retriever.release() }
        }

        return ContainerProbe(
            bytes = file.length(),
            durationMs = durationMs,
            trackWidth = trackWidth,
            trackHeight = trackHeight,
            readableSamples = samples,
        )
    }

    /**
     * Decodes two frames at roughly the quarter and three-quarter marks and counts how many pixels
     * differ. Returns null when either frame cannot be decoded -- an unmeasured comparison, reported
     * as such rather than as "no difference".
     */
    fun measureFrameDifference(file: File): FrameDifference? {
        val retriever = MediaMetadataRetriever()
        var first: Bitmap? = null
        var second: Bitmap? = null
        var firstUs = 0L
        var secondUs = 0L
        try {
            retriever.setDataSource(file.absolutePath)
            val durationRaw = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull()
            if (durationRaw == null || durationRaw <= 0L) return null
            val durationUs = durationRaw * 1000L
            firstUs = durationUs / 4
            secondUs = (durationUs * 3) / 4
            first = retriever.getFrameAtTime(firstUs, MediaMetadataRetriever.OPTION_CLOSEST)
            second = retriever.getFrameAtTime(secondUs, MediaMetadataRetriever.OPTION_CLOSEST)
        } catch (error: Exception) {
            Log.w(TAG, "frame decode failed for ${file.name}: ${error.message}")
            return null
        } finally {
            runCatching { retriever.release() }
        }

        val a = first ?: return null
        val b = second ?: return null
        val width = min(a.width, b.width)
        val height = min(a.height, b.height)
        if (width <= 0 || height <= 0) return null

        val pa = IntArray(width * height)
        val pb = IntArray(width * height)
        a.getPixels(pa, 0, width, 0, 0, width, height)
        b.getPixels(pb, 0, width, 0, 0, width, height)
        var differing = 0
        var maxChannel = 0
        for (i in pa.indices) {
            val x = pa[i]
            val y = pb[i]
            if (x != y) differing++
            /* ARGB packed int; compare each byte channel for the largest single-channel swing. */
            for (shift in intArrayOf(0, 8, 16, 24)) {
                val d = ((x ushr shift) and 0xFF) - ((y ushr shift) and 0xFF)
                val ad = if (d < 0) -d else d
                if (ad > maxChannel) maxChannel = ad
            }
        }
        return FrameDifference(
            firstTimeUs = firstUs,
            secondTimeUs = secondUs,
            differingPixels = differing,
            totalPixels = pa.size,
            maxChannelDelta = maxChannel,
        )
    }

    /** Bits/sec: scale with area and frame rate, bounded so small or large requests stay sane. */
    internal fun bitrateFor(width: Int, height: Int, fps: Int): Int {
        return ((width.toLong() * height.toLong() * fps * 10) / 100).toInt().coerceIn(2_000_000, 24_000_000)
    }
}
