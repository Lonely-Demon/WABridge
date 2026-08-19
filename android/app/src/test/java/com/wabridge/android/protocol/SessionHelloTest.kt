package com.wabridge.android.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class SessionHelloTest {
    @Test
    fun roundTrip() {
        val hello = SessionHello(
            DeviceRole.ANDROID,
            SessionHelloCodec.freshNonce(),
            "phone-test",
            ByteArray(32) { 0xAB.toByte() },
            1024 * 1024,
        )
        assertEquals(hello, SessionHelloCodec.decode(SessionHelloCodec.encode(hello)))
    }

    @Test
    fun noncesAreFresh() {
        assertNotEquals(
            SessionHelloCodec.freshNonce().toList(),
            SessionHelloCodec.freshNonce().toList(),
        )
    }

    @Test
    fun rejectsTruncatedPayload() {
        assertThrows(ProtocolException::class.java) { SessionHelloCodec.decode(ByteArray(10)) }
    }

    @Test
    fun rejectsInvalidDeviceId() {
        assertThrows(IllegalArgumentException::class.java) {
            SessionHello(DeviceRole.WINDOWS, ByteArray(32), "x".repeat(65), ByteArray(32), 1)
        }
    }

    @Test
    fun rejectsInvalidRoleOnWire() {
        val hello = SessionHello(DeviceRole.WINDOWS, ByteArray(32), "desktop", ByteArray(32), 1)
        val payload = SessionHelloCodec.encode(hello)
        payload[0] = 99
        assertThrows(ProtocolException::class.java) { SessionHelloCodec.decode(payload) }
    }
}
