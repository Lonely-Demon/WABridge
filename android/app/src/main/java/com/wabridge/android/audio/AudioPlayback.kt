package com.wabridge.android.audio

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Best-effort low-latency PCM16 sink for the audio channel. Opus frames are
 * deliberately rejected until an audited decoder backend is selected.
 */
class AudioPlayback {
    private var track: AudioTrack? = null
    private val stopped = AtomicBoolean(true)

    @Synchronized
    fun start(sampleRate: Int, channels: Int): Boolean {
        if (sampleRate !in 8_000..192_000 || channels !in 1..2) return false
        stop()
        val channelMask = if (channels == 1) {
            AudioFormat.CHANNEL_OUT_MONO
        } else {
            AudioFormat.CHANNEL_OUT_STEREO
        }
        val minimum = AudioTrack.getMinBufferSize(
            sampleRate,
            channelMask,
            AudioFormat.ENCODING_PCM_16BIT,
        )
        if (minimum <= 0) return false
        val created = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setSampleRate(sampleRate)
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setChannelMask(channelMask)
                    .build(),
            )
            .setBufferSizeInBytes(minimum.coerceAtLeast(sampleRate * channels / 4))
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        if (created.state != AudioTrack.STATE_INITIALIZED) {
            created.release()
            return false
        }
        track = created
        created.play()
        stopped.set(false)
        return true
    }

    @Synchronized
    fun play(frame: AudioFrame): Boolean {
        if (frame.codec != AudioCodec.PCM16 || frame.channels !in 1..2) return false
        if (stopped.get() || track == null) {
            if (!start(frame.sampleRate, frame.channels)) return false
        }
        val destination = track ?: return false
        return runCatching {
            var offset = 0
            while (offset < frame.data.size) {
                val written = destination.write(frame.data, offset, frame.data.size - offset)
                if (written <= 0) return false
                offset += written
            }
            true
        }.getOrDefault(false)
    }

    @Synchronized
    fun stop() {
        if (!stopped.compareAndSet(false, true)) {
            track?.release()
            track = null
            return
        }
        track?.runCatching {
            pause()
            flush()
            release()
        }
        track = null
    }
}
