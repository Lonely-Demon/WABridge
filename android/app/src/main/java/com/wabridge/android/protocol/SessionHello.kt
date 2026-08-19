package com.wabridge.android.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.SecureRandom

enum class DeviceRole(val wireValue: Byte) {
    WINDOWS(1),
    ANDROID(2),
}

data class SessionHello(
    val role: DeviceRole,
    val sessionNonce: ByteArray,
    val deviceId: String,
    val capabilitiesHash: ByteArray,
    val maxFrame: Int,
) {
    init {
        require(sessionNonce.size == 32) { "session nonce must be 32 bytes" }
        require(capabilitiesHash.size == 32) { "capability hash must be 32 bytes" }
        require(deviceId.isNotEmpty() && deviceId.length <= 64) { "device id length is invalid" }
        require(deviceId.all { it.code in 0x20..0x7e }) { "device id contains invalid characters" }
        require(maxFrame in 1..(4 * 1024 * 1024)) { "max frame is invalid" }
    }

    override fun equals(other: Any?): Boolean =
        other is SessionHello && role == other.role && deviceId == other.deviceId &&
            maxFrame == other.maxFrame && sessionNonce.contentEquals(other.sessionNonce) &&
            capabilitiesHash.contentEquals(other.capabilitiesHash)

    override fun hashCode(): Int = 31 * (31 * (31 * role.hashCode() + deviceId.hashCode()) + maxFrame) +
        31 * sessionNonce.contentHashCode() + capabilitiesHash.contentHashCode()
}

object SessionHelloCodec {
    private const val FIXED_OVERHEAD = 1 + 32 + 1 + 32 + 4

    fun encode(hello: SessionHello): ByteArray {
        val id = hello.deviceId.toByteArray(Charsets.US_ASCII)
        require(id.size <= 64) { "device id is too long" }
        return ByteBuffer.allocate(FIXED_OVERHEAD + id.size)
            .order(ByteOrder.BIG_ENDIAN)
            .put(hello.role.wireValue)
            .put(hello.sessionNonce)
            .put(id.size.toByte())
            .put(id)
            .put(hello.capabilitiesHash)
            .putInt(hello.maxFrame)
            .array()
    }

    fun decode(payload: ByteArray): SessionHello {
        if (payload.size < FIXED_OVERHEAD) throw ProtocolException("truncated session hello")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN)
        val role = when (buffer.get()) {
            DeviceRole.WINDOWS.wireValue -> DeviceRole.WINDOWS
            DeviceRole.ANDROID.wireValue -> DeviceRole.ANDROID
            else -> throw ProtocolException("invalid hello role")
        }
        val nonce = ByteArray(32).also(buffer::get)
        val idLength = buffer.get().toInt() and 0xff
        if (idLength == 0 || idLength > 64 || payload.size != FIXED_OVERHEAD + idLength) {
            throw ProtocolException("invalid hello device id length")
        }
        val id = ByteArray(idLength).also(buffer::get).toString(Charsets.US_ASCII)
        val capabilities = ByteArray(32).also(buffer::get)
        val maxFrame = buffer.int
        return try {
            SessionHello(role, nonce, id, capabilities, maxFrame)
        } catch (error: IllegalArgumentException) {
            throw ProtocolException(error.message ?: "invalid session hello")
        }
    }

    fun freshNonce(): ByteArray = ByteArray(32).also(SecureRandom()::nextBytes)
}
