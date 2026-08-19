package com.wabridge.android.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class EnvelopeTest {
    @Test
    fun roundTrip() {
        val original = Envelope(1, 1, 1, 42L, "hello".toByteArray())
        assertEquals(original, EnvelopeCodec.decode(EnvelopeCodec.encode(original)))
    }

    @Test
    fun rejectsTruncatedHeader() {
        assertThrows(ProtocolException::class.java) { EnvelopeCodec.decode(byteArrayOf(0x57, 0x42)) }
    }

    @Test
    fun rejectsBadMagic() {
        val frame = EnvelopeCodec.encode(Envelope(1, 1, 1, 1L, byteArrayOf(1)))
        frame[0] = 0
        assertThrows(ProtocolException::class.java) { EnvelopeCodec.decode(frame) }
    }

    @Test
    fun rejectsZeroRequestId() {
        assertThrows(ProtocolException::class.java) {
            EnvelopeCodec.encode(Envelope(1, 1, 1, 0L, byteArrayOf(1)))
        }
    }

    @Test
    fun rejectsOversizedPayload() {
        assertThrows(ProtocolException::class.java) {
            EnvelopeCodec.encode(Envelope(1, 1, 1, 1L, ByteArray(64 * 1024 + 1)))
        }
    }
}
