package com.wabridge.android.session

import android.net.Uri
import java.net.URI
import java.net.URLDecoder
import java.nio.charset.StandardCharsets
import java.util.Locale

/** Non-secret QR bootstrap data; TLS and first-pair approval remain authoritative. */
data class QrPairingPayload(
    val host: String,
    val port: Int,
    val deviceId: String,
    val fingerprint: String,
    val expiresAt: Long,
) {
    fun isExpired(nowSeconds: Long = System.currentTimeMillis() / 1000L): Boolean = expiresAt < nowSeconds

    companion object {
        private val FINGERPRINT = Regex("^[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){31}$")
        private val DEVICE_ID = Regex("^[A-Za-z0-9._-]{1,64}$")
        private val NONCE = Regex("^[0-9a-fA-F]{8,32}$")

        fun parse(uri: Uri, nowSeconds: Long = System.currentTimeMillis() / 1000L): QrPairingPayload =
            parse(uri.toString(), nowSeconds)

        fun parse(raw: String, nowSeconds: Long = System.currentTimeMillis() / 1000L): QrPairingPayload {
            val uri = URI(raw)
            require(uri.scheme?.lowercase(Locale.US) == "wabridge") { "Not a WABridge QR payload" }
            require(uri.host?.lowercase(Locale.US) == "pair") { "Unsupported WABridge QR action" }
            val values = linkedMapOf<String, String>()
            uri.rawQuery.orEmpty().split('&').filter { it.isNotEmpty() }.forEach { part ->
                val pieces = part.split('=', limit = 2)
                require(pieces.size == 2) { "Malformed QR parameter" }
                val key = URLDecoder.decode(pieces[0], StandardCharsets.UTF_8.name())
                require(key !in values) { "Duplicate QR parameter" }
                values[key] = URLDecoder.decode(pieces[1], StandardCharsets.UTF_8.name())
            }
            require(values["v"] == "1") { "Unsupported WABridge QR version" }
            val host = values["host"]?.trim().orEmpty()
            require(host.isNotEmpty() && host.length <= 253 && !host.contains(" ")) { "Invalid Windows host" }
            val port = values["port"]?.toIntOrNull() ?: error("Invalid Windows port")
            require(port in 1..65535) { "Invalid Windows port" }
            val deviceId = values["device_id"]?.trim().orEmpty()
            require(DEVICE_ID.matches(deviceId)) { "Invalid Windows device ID" }
            val fingerprint = values["fp"]?.trim().orEmpty()
            require(FINGERPRINT.matches(fingerprint)) { "Invalid Windows certificate fingerprint" }
            val expiresAt = values["expires"]?.toLongOrNull() ?: error("Invalid QR expiry")
            require(expiresAt >= nowSeconds && expiresAt <= nowSeconds + 900) { "Expired or unsafe QR payload" }
            require(NONCE.matches(values["nonce"]?.trim().orEmpty())) { "Invalid QR nonce" }
            require(values.keys.none { it.equals("key", true) || it.contains("private", true) || it.contains("secret", true) }) {
                "QR payload contains a forbidden secret field"
            }
            return QrPairingPayload(host, port, deviceId, fingerprint.uppercase(Locale.US), expiresAt)
        }
    }
}
