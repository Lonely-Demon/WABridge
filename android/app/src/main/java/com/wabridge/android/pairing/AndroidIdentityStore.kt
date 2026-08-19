package com.wabridge.android.pairing

import android.content.Context
import android.content.SharedPreferences
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.security.KeyPairGenerator
import java.security.KeyStore
import javax.net.ssl.KeyManagerFactory
import java.security.MessageDigest
import java.security.PrivateKey
import java.security.Signature
import java.security.cert.X509Certificate
import java.security.spec.ECGenParameterSpec

class AndroidIdentityStore(context: Context, private val alias: String = "wabridge-device-v1") {
    private val keyStore: KeyStore = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
    private val preferences: SharedPreferences =
        context.getSharedPreferences("wabridge-pairing", Context.MODE_PRIVATE)

    fun ensureIdentity(): X509Certificate {
        if (!keyStore.containsAlias(alias)) {
            val generator = KeyPairGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_EC,
                "AndroidKeyStore",
            )
            generator.initialize(
                KeyGenParameterSpec.Builder(
                    alias,
                    KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_VERIFY,
                )
                    .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
                    .setDigests(KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_SHA512)
                    .setUserAuthenticationRequired(false)
                    .build(),
            )
            generator.generateKeyPair()
        }
        return certificate()
    }

    fun certificate(): X509Certificate =
        (keyStore.getCertificate(alias) as? X509Certificate)
            ?: error("WABridge identity certificate is missing")

    fun fingerprint(): String = certificate().publicKey.encoded
        .let { MessageDigest.getInstance("SHA-256").digest(it) }
        .joinToString(":") { "%02X".format(it) }

    fun keyManagers(): Array<javax.net.ssl.KeyManager> {
        val factory = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm())
        factory.init(keyStore, null)
        return factory.keyManagers
    }

    fun sign(data: ByteArray): ByteArray {
        val entry = keyStore.getEntry(alias, null) as? KeyStore.PrivateKeyEntry
            ?: error("WABridge identity private key is missing")
        return Signature.getInstance("SHA256withECDSA").run {
            initSign(entry.privateKey)
            update(data)
            sign()
        }
    }

    fun rememberPeer(deviceId: String, fingerprint: String) {
        require(deviceId.isNotBlank() && fingerprint.isNotBlank())
        preferences.edit().putString("peer.$deviceId", fingerprint).apply()
    }

    fun peerFingerprint(deviceId: String): String? = preferences.getString("peer.$deviceId", null)

    fun revokePeer(deviceId: String) {
        preferences.edit().remove("peer.$deviceId").apply()
    }
}
