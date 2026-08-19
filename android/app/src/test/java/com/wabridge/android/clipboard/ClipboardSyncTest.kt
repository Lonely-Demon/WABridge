package com.wabridge.android.clipboard

import com.wabridge.android.protocol.ProtocolException
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertThrows
import org.junit.Test

class ClipboardSyncTest {
    @Test
    fun roundTrip() {
        val update = ClipboardUpdate(ByteArray(16) { 1 }, "phone", 1234L, "hello WABridge")
        val decoded = ClipboardCodec.decode(ClipboardCodec.encode(update))
        assertArrayEquals(update.loopToken, decoded.loopToken)
        assertEquals(update.originDeviceId, decoded.originDeviceId)
        assertEquals(update.timestampMs, decoded.timestampMs)
        assertEquals(update.text, decoded.text)
    }

    @Test
    fun suppressesLoops() {
        val guard = ClipboardLoopGuard()
        val token = ByteArray(16) { 2 }
        assertTrue(guard.shouldApply(token))
        assertFalse(guard.shouldApply(token))
        guard.markLocal(token)
        assertFalse(guard.shouldApply(token))
    }

    @Test
    fun rejectsTruncatedPayload() {
        assertThrows(ProtocolException::class.java) { ClipboardCodec.decode(ByteArray(10)) }
    }
}
