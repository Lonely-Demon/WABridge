package com.wabridge.android.input

import com.wabridge.android.protocol.ProtocolException
import java.nio.ByteBuffer
import java.nio.ByteOrder

enum class InputEventType(val wireValue: Byte) {
    MOUSE_MOVE(1),
    MOUSE_BUTTON(2),
    MOUSE_WHEEL(3),
    KEY(4),
}

data class InputEvent(
    val type: InputEventType,
    val flags: Int,
    val modifiers: Int,
    val x: Int,
    val y: Int,
    val wheel: Int,
    val code: Long,
    val button: Int,
) {
    init {
        require(flags in 0..255)
        require(modifiers in 0..65535)
        require(x in -32_767..32_767 && y in -32_767..32_767)
        require(wheel in -1_000_000..1_000_000)
        require(button in 0..255)
        require(code in 0..0x10FFFF)
        if (type == InputEventType.MOUSE_BUTTON) {
            require(button in 1..8)
            require(flags == 1 || flags == 2)
        }
        if (type == InputEventType.KEY) {
            require(code > 0)
            require(flags == 1 || flags == 2)
        }
        if (type == InputEventType.MOUSE_MOVE || type == InputEventType.MOUSE_WHEEL) require(flags == 0)
    }
}

object InputEventCodec {
    const val SIZE = 21

    fun encode(event: InputEvent): ByteArray = ByteBuffer.allocate(SIZE)
        .order(ByteOrder.BIG_ENDIAN)
        .put(event.type.wireValue)
        .put(event.flags.toByte())
        .putShort(event.modifiers.toShort())
        .putInt(event.x)
        .putInt(event.y)
        .putInt(event.wheel)
        .putInt(event.code.toInt())
        .put(event.button.toByte())
        .array()

    fun decode(payload: ByteArray): InputEvent {
        if (payload.size != SIZE) throw ProtocolException("invalid input event size")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN)
        val type = when (buffer.get()) {
            InputEventType.MOUSE_MOVE.wireValue -> InputEventType.MOUSE_MOVE
            InputEventType.MOUSE_BUTTON.wireValue -> InputEventType.MOUSE_BUTTON
            InputEventType.MOUSE_WHEEL.wireValue -> InputEventType.MOUSE_WHEEL
            InputEventType.KEY.wireValue -> InputEventType.KEY
            else -> throw ProtocolException("invalid input event type")
        }
        val flags = buffer.get().toInt() and 0xff
        val modifiers = buffer.short.toInt() and 0xffff
        val x = buffer.int
        val y = buffer.int
        val wheel = buffer.int
        val code = buffer.int.toLong() and 0xffff_ffffL
        val button = buffer.get().toInt() and 0xff
        return try {
            InputEvent(type, flags, modifiers, x, y, wheel, code, button)
        } catch (error: IllegalArgumentException) {
            throw ProtocolException(error.message ?: "invalid input event")
        }
    }
}
