package com.wabridge.android.pairing

import java.security.MessageDigest
import java.security.SecureRandom
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.X509TrustManager

object PinnedTls {
    /** Compatibility API for callers that already hold the complete peer certificate. */
    fun context(
        identity: AndroidIdentityStore,
        peerCertificate: X509Certificate?,
        firstPair: Boolean,
    ): SSLContext = contextWithFingerprint(
        identity,
        peerCertificate?.let(::fingerprint),
        firstPair,
    )

    /**
     * Builds a TLS 1.3 context that either permits the explicit first-pair flow
     * or compares the peer certificate public-key fingerprint exactly.
     */
    fun contextWithFingerprint(
        identity: AndroidIdentityStore,
        pinnedFingerprint: String?,
        firstPair: Boolean,
    ): SSLContext {
        require(firstPair || !pinnedFingerprint.isNullOrBlank()) {
            "pinned mode requires a peer fingerprint"
        }
        val trustManager = object : X509TrustManager {
            override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()

            override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {
                if (chain.isEmpty()) throw CertificateException("client certificate chain is empty")
            }

            override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {
                if (chain.isEmpty()) throw CertificateException("server certificate chain is empty")
                if (!firstPair) {
                    val actual = fingerprint(chain[0])
                    if (actual != pinnedFingerprint) {
                        throw CertificateException("WABridge peer certificate changed")
                    }
                }
            }
        }
        return SSLContext.getInstance("TLSv1.3").apply {
            init(identity.keyManagers(), arrayOf(trustManager), SecureRandom())
        }
    }

    fun configure(socket: SSLSocket) {
        socket.enabledProtocols = arrayOf("TLSv1.3")
        socket.useClientMode = true
        socket.enableSessionCreation = true
    }

    fun fingerprint(certificate: X509Certificate): String = certificate.publicKey.encoded
        .let { MessageDigest.getInstance("SHA-256").digest(it) }
        .joinToString(":") { "%02X".format(it) }
}
