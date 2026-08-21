package com.wabridge.android.session

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class QrPairingTest {
    private val fingerprint = "AA" + ":BB".repeat(31)

    @Test
    fun acceptsBoundedFreshPayload() {
        val raw = "wabridge://pair?v=1&host=192.168.1.20&port=51820&device_id=DESKTOP-1&fp=$fingerprint&expires=1000&nonce=abcdef12"
        val payload = QrPairingPayload.parse(raw, nowSeconds = 900)
        assertEquals("192.168.1.20", payload.host)
        assertEquals(51820, payload.port)
        assertEquals(fingerprint, payload.fingerprint)
    }

    @Test
    fun rejectsExpiredPayload() {
        val raw = "wabridge://pair?v=1&host=192.168.1.20&port=51820&device_id=DESKTOP-1&fp=$fingerprint&expires=899&nonce=abcdef12"
        assertThrows(IllegalArgumentException::class.java) { QrPairingPayload.parse(raw, nowSeconds = 900) }
    }

    @Test
    fun rejectsMalformedFingerprint() {
        val raw = "wabridge://pair?v=1&host=192.168.1.20&port=51820&device_id=DESKTOP-1&fp=AA:BB&expires=1000&nonce=abcdef12"
        assertThrows(IllegalArgumentException::class.java) { QrPairingPayload.parse(raw, nowSeconds = 900) }
    }

    @Test
    fun rejectsMalformedNonce() {
        val raw = "wabridge://pair?v=1&host=192.168.1.20&port=51820&device_id=DESKTOP-1&fp=$fingerprint&expires=1000&nonce=short"
        assertThrows(IllegalArgumentException::class.java) { QrPairingPayload.parse(raw, nowSeconds = 900) }
    }

    @Test
    fun rejectsSecretFields() {
        val raw = "wabridge://pair?v=1&host=192.168.1.20&port=51820&device_id=DESKTOP-1&fp=$fingerprint&expires=1000&nonce=abcdef12&private_key=bad"
        assertThrows(IllegalArgumentException::class.java) { QrPairingPayload.parse(raw, nowSeconds = 900) }
    }
}
