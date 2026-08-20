package com.wabridge.android.audio

import com.wabridge.android.protocol.ProtocolException
import java.nio.ByteBuffer
import java.nio.ByteOrder

enum class AudioCodec(val wireValue: Byte) {
    PCM16(1),
    OPUS(2),
}

data class AudioFrame(
    val codec: AudioCodec,
    val channels: Int,
    val sampleRate: Int,
    val sequence: Long,
    val timestampMs: Long,
    val data: ByteArray,
) {
    init {
        require(channels in 1..8)
        require(sampleRate in 8_000..192_000)
        require(timestampMs > 0)
        require(data.isNotEmpty() && data.size <= AudioCodecWire.MAX_DATA)
        if (codec == AudioCodec.PCM16) require(data.size % (2 * channels) == 0)
    }
}

object AudioCodecWire {
    const val HEADER_SIZE = 26
    const val MAX_DATA = 256 * 1024

    fun encode(frame: AudioFrame): ByteArray = ByteBuffer.allocate(HEADER_SIZE + frame.data.size)
        .order(ByteOrder.BIG_ENDIAN)
        .put(frame.codec.wireValue)
        .put(frame.channels.toByte())
        .putInt(frame.sampleRate)
        .putLong(frame.sequence)
        .putLong(frame.timestampMs)
        .putInt(frame.data.size)
        .put(frame.data)
        .array()

    fun decode(payload: ByteArray): AudioFrame {
        if (payload.size < HEADER_SIZE) throw ProtocolException("truncated audio frame")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN)
        val codec = when (buffer.get()) {
            AudioCodec.PCM16.wireValue -> AudioCodec.PCM16
            AudioCodec.OPUS.wireValue -> AudioCodec.OPUS
            else -> throw ProtocolException("invalid audio codec")
        }
        val channels = buffer.get().toInt() and 0xff
        val sampleRate = buffer.int
        val sequence = buffer.long
        val timestamp = buffer.long
        val length = buffer.int
        if (length <= 0 || length > MAX_DATA || buffer.remaining() != length) {
            throw ProtocolException("invalid audio payload length")
        }
        return try {
            AudioFrame(codec, channels, sampleRate, sequence, timestamp, ByteArray(length).also(buffer::get))
        } catch (error: IllegalArgumentException) {
            throw ProtocolException(error.message ?: "invalid audio frame")
        }
    }
}
