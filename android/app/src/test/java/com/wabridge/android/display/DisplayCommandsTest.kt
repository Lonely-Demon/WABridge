package com.wabridge.android.display

import com.wabridge.android.protocol.ProtocolException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertThrows
import org.junit.Test

class DisplayCommandsTest {
    @Test
    fun roundTripsPhoneControlCommand() {
        val command = DisplayCommand(DisplayMode.PHONE_CONTROL, true, 0x01020304)
        val decoded = DisplayCommandCodec.decode(DisplayCommandCodec.encode(command))
        assertEquals(command, decoded)
    }

    @Test
    fun rejectsMalformedCommands() {
        assertThrows(ProtocolException::class.java) { DisplayCommandCodec.decode(byteArrayOf(1, 0)) }
        assertThrows(ProtocolException::class.java) {
            DisplayCommandCodec.decode(byteArrayOf(9, 0, 0, 0, 0, 1))
        }
        assertThrows(ProtocolException::class.java) {
            DisplayCommandCodec.decode(byteArrayOf(1, 2, 0, 0, 0, 1))
        }
        assertThrows(ProtocolException::class.java) {
            DisplayCommandCodec.decode(byteArrayOf(1, 0, 0, 0, 0, 0))
        }
        assertTrue(DisplayCommandCodec.encode(DisplayCommand(DisplayMode.SECOND_DISPLAY, false, 1)).size == 6)
    }
}
