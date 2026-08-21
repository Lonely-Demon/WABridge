package com.wabridge.android.session

import android.net.Uri
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

        fun parse(uri: Uri, nowSeconds: Long = System.currentTimeMillis() / 1000L): QrPairingPayload {
            require(uri.scheme?.lowercase(Locale.US) == "wabridge") { "Not a WABridge QR payload" }
            require(uri.host?.lowercase(Locale.US) == "pair") { "Unsupported WABridge QR action" }
            require(uri.getQueryParameter("v") == "1") { "Unsupported WABridge QR version" }
            val host = uri.getQueryParameter("host")?.trim().orEmpty()
            require(host.isNotEmpty() && host.length <= 253 && !host.contains(" ")) { "Invalid Windows host" }
            val port = uri.getQueryParameter("port")?.toIntOrNull() ?: error("Invalid Windows port")
            require(port in 1..65535) { "Invalid Windows port" }
            val deviceId = uri.getQueryParameter("device_id")?.trim().orEmpty()
            require(DEVICE_ID.matches(deviceId)) { "Invalid Windows device ID" }
            val fingerprint = uri.getQueryParameter("fp")?.trim().orEmpty()
            require(FINGERPRINT.matches(fingerprint)) { "Invalid Windows certificate fingerprint" }
            val expiresAt = uri.getQueryParameter("expires")?.toLongOrNull() ?: error("Invalid QR expiry")
            require(expiresAt >= nowSeconds && expiresAt <= nowSeconds + 900) { "Expired or unsafe QR payload" }
            val nonce = uri.getQueryParameter("nonce")?.trim().orEmpty()
            require(nonce.matches(Regex("^[0-9a-fA-F]{8,32}$"))) { "Invalid QR nonce" }
            require(uri.queryParameterNames.none { it.equals("key", true) || it.contains("private", true) || it.contains("secret", true) }) {
                "QR payload contains a forbidden secret field"
            }
            return QrPairingPayload(host, port, deviceId, fingerprint.uppercase(Locale.US), expiresAt)
        }
    }
}
