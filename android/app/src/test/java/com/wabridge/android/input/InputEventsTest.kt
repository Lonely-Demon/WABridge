package com.wabridge.android.input

import com.wabridge.android.protocol.ProtocolException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class InputEventsTest {
    @Test
    fun roundTripsMouseAndKeyboardEvents() {
        val move = InputEvent(InputEventType.MOUSE_MOVE, 0, 3, 120, -45, 0, 0, 0)
        val decodedMove = InputEventCodec.decode(InputEventCodec.encode(move))
        assertEquals(move.type, decodedMove.type)
        assertEquals(move.modifiers, decodedMove.modifiers)
        assertEquals(move.x, decodedMove.x)
        assertEquals(move.y, decodedMove.y)

        val key = InputEvent(InputEventType.KEY, 1, 4, 0, 0, 0, 0x41, 0)
        val decodedKey = InputEventCodec.decode(InputEventCodec.encode(key))
        assertEquals(key.type, decodedKey.type)
        assertEquals(key.flags, decodedKey.flags)
        assertEquals(key.code, decodedKey.code)

        val button = InputEvent(InputEventType.MOUSE_BUTTON, 0, 0, 0, 0, 0, 0, 1)
        assertEquals(1, InputEventCodec.decode(InputEventCodec.encode(button)).button)
    }

    @Test
    fun rejectsInvalidEventsAndPayloads() {
        assertThrows(IllegalArgumentException::class.java) {
            InputEvent(InputEventType.MOUSE_MOVE, 0, 0, 40_000, 0, 0, 0, 0)
        }
        assertThrows(IllegalArgumentException::class.java) {
            InputEvent(InputEventType.MOUSE_BUTTON, 0, 0, 0, 0, 0, 0, 0)
        }
        assertThrows(IllegalArgumentException::class.java) {
            InputEvent(InputEventType.KEY, 0, 0, 0, 0, 0, 0, 0)
        }
        assertThrows(ProtocolException::class.java) {
            InputEventCodec.decode(ByteArray(InputEventCodec.SIZE - 1))
        }
    }
}
