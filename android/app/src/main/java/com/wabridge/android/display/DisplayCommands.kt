package com.wabridge.android.display

import com.wabridge.android.protocol.ProtocolException
import java.nio.ByteBuffer
import java.nio.ByteOrder

enum class DisplayMode(val wire: Int) {
    NONE(0),
    SECOND_DISPLAY(1),
    PHONE_CONTROL(2),
}

data class DisplayCommand(
    val mode: DisplayMode,
    val suspended: Boolean,
    val sequence: Long,
) {
    init {
        require(sequence in 1..0xffffffffL)
    }
}

object DisplayCommandCodec {
    fun encode(command: DisplayCommand): ByteArray {
        if (command.sequence !in 1..0xffffffffL) throw ProtocolException("invalid display sequence")
        return ByteBuffer.allocate(6)
            .order(ByteOrder.BIG_ENDIAN)
            .put(command.mode.wire.toByte())
            .put(if (command.suspended) 1 else 0)
            .putInt(command.sequence.toInt())
            .array()
    }

    fun decode(payload: ByteArray): DisplayCommand {
        if (payload.size != 6) throw ProtocolException("invalid display command length")
        val mode = DisplayMode.entries.firstOrNull { it.wire == (payload[0].toInt() and 0xff) }
            ?: throw ProtocolException("invalid display mode")
        val suspended = when (payload[1].toInt() and 0xff) {
            0 -> false
            1 -> true
            else -> throw ProtocolException("invalid display suspend flag")
        }
        val sequence = ByteBuffer.wrap(payload, 2, 4).order(ByteOrder.BIG_ENDIAN).int.toLong() and 0xffffffffL
        if (sequence == 0L) throw ProtocolException("invalid display sequence")
        return DisplayCommand(mode, suspended, sequence)
    }
}
