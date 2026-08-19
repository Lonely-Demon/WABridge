package com.wabridge.android.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

class ProtocolException(message: String) : IllegalArgumentException(message)

data class Envelope(
    val channel: Int,
    val kind: Int,
    val flags: Int,
    val requestId: Long,
    val payload: ByteArray,
) {
    override fun equals(other: Any?): Boolean =
        other is Envelope && channel == other.channel && kind == other.kind &&
            flags == other.flags && requestId == other.requestId && payload.contentEquals(other.payload)

    override fun hashCode(): Int = 31 * (31 * (31 * (31 * channel + kind) + flags) + requestId.hashCode()) + payload.contentHashCode()
}

object EnvelopeCodec {
    private const val MAGIC: Short = 0x5742
    private const val VERSION: Byte = 1
    private const val HEADER_SIZE = 20
    private val channelLimits = mapOf(
        1 to 64 * 1024,
        2 to 4 * 1024 * 1024,
        3 to 1 * 1024 * 1024,
        4 to 1 * 1024 * 1024,
        5 to 256 * 1024,
    )

    fun encode(envelope: Envelope): ByteArray {
        val limit = channelLimits[envelope.channel] ?: throw ProtocolException("unknown channel")
        require(envelope.kind in 0..0xffff) { "kind out of range" }
        require(envelope.flags in 0..0xffff) { "flags out of range" }
        if (envelope.requestId == 0L) throw ProtocolException("zero request id")
        if (envelope.payload.isEmpty() || envelope.payload.size > limit) {
            throw ProtocolException("payload exceeds channel limit")
        }
        return ByteBuffer.allocate(HEADER_SIZE + envelope.payload.size)
            .order(ByteOrder.BIG_ENDIAN)
            .putShort(MAGIC)
            .put(VERSION)
            .put(envelope.channel.toByte())
            .putShort(envelope.kind.toShort())
            .putShort(envelope.flags.toShort())
            .putLong(envelope.requestId)
            .putInt(envelope.payload.size)
            .put(envelope.payload)
            .array()
    }

    fun decode(frame: ByteArray): Envelope {
        if (frame.size < HEADER_SIZE) throw ProtocolException("truncated header")
        val buffer = ByteBuffer.wrap(frame).order(ByteOrder.BIG_ENDIAN)
        if (buffer.short != MAGIC) throw ProtocolException("bad magic")
        if (buffer.get() != VERSION) throw ProtocolException("unsupported version")
        val channel = buffer.get().toInt() and 0xff
        val limit = channelLimits[channel] ?: throw ProtocolException("unknown channel")
        val kind = buffer.short.toInt() and 0xffff
        val flags = buffer.short.toInt() and 0xffff
        val requestId = buffer.long
        val length = buffer.int
        if (requestId == 0L) throw ProtocolException("zero request id")
        if (length <= 0 || length > limit) throw ProtocolException("invalid payload length")
        if (frame.size != HEADER_SIZE + length) throw ProtocolException("frame length mismatch")
        val payload = ByteArray(length)
        buffer.get(payload)
        return Envelope(channel, kind, flags, requestId, payload)
    }
}
