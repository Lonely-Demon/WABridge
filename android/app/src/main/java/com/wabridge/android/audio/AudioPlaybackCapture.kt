package com.wabridge.android.audio

import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Captures device playback audio only after explicit MediaProjection consent.
 * This class never requests or fabricates user consent and does not capture
 * microphone input.
 */
class AudioPlaybackCapture(private val context: Context) {
    private val stopped = AtomicBoolean(true)
    private var captureJob: Job? = null
    private var record: AudioRecord? = null
    private var projection: MediaProjection? = null
    private var sequence = 0L

    fun consentIntent(): Intent = context.getSystemService(MediaProjectionManager::class.java)
        .createScreenCaptureIntent()

    @Synchronized
    fun start(
        resultCode: Int,
        resultData: Intent,
        sampleRate: Int = 48_000,
        channels: Int = 2,
        onFrame: (AudioFrame) -> Unit,
    ): Boolean {
        if (sampleRate !in 8_000..192_000 || channels !in 1..2) return false
        stop()
        val manager = context.getSystemService(MediaProjectionManager::class.java)
        val createdProjection = runCatching { manager.getMediaProjection(resultCode, resultData) }.getOrNull() ?: return false
        val channelMask = if (channels == 1) AudioFormat.CHANNEL_IN_MONO else AudioFormat.CHANNEL_IN_STEREO
        val format = AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setSampleRate(sampleRate)
            .setChannelMask(channelMask)
            .build()
        val captureConfig = AudioPlaybackCaptureConfiguration.Builder(createdProjection)
            .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
            .addMatchingUsage(AudioAttributes.USAGE_GAME)
            .build()
        val minimum = AudioRecord.getMinBufferSize(sampleRate, channelMask, AudioFormat.ENCODING_PCM_16BIT)
        if (minimum <= 0) {
            createdProjection.stop()
            return false
        }
        val createdRecord = runCatching {
            AudioRecord.Builder()
                .setAudioFormat(format)
                .setBufferSizeInBytes(minimum.coerceAtLeast(sampleRate * channels / 2))
                .setAudioPlaybackCaptureConfig(captureConfig)
                .build()
        }.getOrNull()
        if (createdRecord == null || createdRecord.state != AudioRecord.STATE_INITIALIZED) {
            createdRecord?.release()
            createdProjection.stop()
            return false
        }
        projection = createdProjection
        record = createdRecord
        sequence = 0L
        stopped.set(false)
        createdRecord.startRecording()
        captureJob = CoroutineScope(Dispatchers.IO).launch {
            val bytes = ByteArray((sampleRate * channels / 10).coerceAtLeast(1024) * 2)
            while (isActive && !stopped.get()) {
                val read = createdRecord.read(bytes, 0, bytes.size, AudioRecord.READ_BLOCKING)
                if (read <= 0) break
                val payload = ByteBuffer.allocate(read).order(ByteOrder.LITTLE_ENDIAN).put(bytes, 0, read).array()
                onFrame(AudioFrame(AudioCodec.PCM16, channels, sampleRate, sequence++, System.currentTimeMillis(), payload))
            }
        }
        return true
    }

    @Synchronized
    fun stop() {
        stopped.set(true)
        captureJob?.cancel()
        captureJob = null
        record?.runCatching {
            stop()
            release()
        }
        record = null
        projection?.stop()
        projection = null
    }
}
