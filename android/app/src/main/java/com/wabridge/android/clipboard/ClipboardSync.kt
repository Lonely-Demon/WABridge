package com.wabridge.android.clipboard

import com.wabridge.android.protocol.ProtocolException
import java.nio.ByteBuffer
import java.nio.ByteOrder

class ClipboardUpdate(
    val loopToken: ByteArray,
    val originDeviceId: String,
    val timestampMs: Long,
    val text: String,
) {
    init {
        require(loopToken.size == 16)
        require(originDeviceId.isNotEmpty() && originDeviceId.length <= 64)
        require(originDeviceId.all { it.code in 0x20..0x7e })
        require(timestampMs > 0)
        require(text.isNotEmpty() && text.toByteArray().size <= ClipboardCodec.MAX_TEXT)
    }
}

object ClipboardCodec {
    fun encode(update: ClipboardUpdate): ByteArray {
        val device = update.originDeviceId.toByteArray(Charsets.US_ASCII)
        val text = update.text.toByteArray(Charsets.UTF_8)
        if (device.size > 64 || text.isEmpty() || text.size > MAX_TEXT) {
            throw ProtocolException("invalid clipboard update")
        }
        return ByteBuffer.allocate(16 + 8 + 1 + device.size + 4 + text.size)
            .order(ByteOrder.BIG_ENDIAN)
            .put(update.loopToken)
            .putLong(update.timestampMs)
            .put(device.size.toByte())
            .put(device)
            .putInt(text.size)
            .put(text)
            .array()
    }

    fun decode(payload: ByteArray): ClipboardUpdate {
        if (payload.size < 29) throw ProtocolException("truncated clipboard update")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN)
        val token = ByteArray(16).also(buffer::get)
        val timestamp = buffer.long
        val deviceLength = buffer.get().toInt() and 0xff
        if (deviceLength == 0 || deviceLength > 64 || buffer.remaining() < deviceLength + 4) {
            throw ProtocolException("invalid clipboard device id length")
        }
        val device = ByteArray(deviceLength).also(buffer::get).toString(Charsets.US_ASCII)
        val textLength = buffer.int
        if (textLength <= 0 || textLength > MAX_TEXT || buffer.remaining() != textLength) {
            throw ProtocolException("invalid clipboard text length")
        }
        val text = ByteArray(textLength).also(buffer::get).toString(Charsets.UTF_8)
        return try {
            ClipboardUpdate(token, device, timestamp, text)
        } catch (error: IllegalArgumentException) {
            throw ProtocolException(error.message ?: "invalid clipboard update")
        }
    }

    const val MAX_TEXT = 1024 * 1024
}

class ClipboardLoopGuard {
    private var lastToken: ByteArray? = null

    fun shouldApply(token: ByteArray): Boolean {
        val previous = lastToken
        if (previous != null && previous.contentEquals(token)) return false
        lastToken = token.copyOf()
        return true
    }

    fun markLocal(token: ByteArray) {
        lastToken = token.copyOf()
    }
}
