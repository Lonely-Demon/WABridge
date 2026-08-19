package com.wabridge.android.session

import com.wabridge.android.pairing.AndroidIdentityStore
import com.wabridge.android.pairing.PinnedTls
import com.wabridge.android.protocol.DeviceRole
import com.wabridge.android.protocol.Envelope
import com.wabridge.android.protocol.EnvelopeCodec
import com.wabridge.android.protocol.ProtocolException
import com.wabridge.android.protocol.SessionHello
import com.wabridge.android.protocol.SessionHelloCodec
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.net.InetSocketAddress
import java.security.cert.X509Certificate
import java.util.concurrent.atomic.AtomicBoolean
import javax.net.ssl.SSLSocket

class TlsSessionClient(
    private val identity: AndroidIdentityStore,
    private val deviceId: String,
    private val capabilitiesHash: ByteArray,
) {
    data class Peer(
        val certificate: X509Certificate,
        val fingerprint: String,
        val hello: SessionHello,
    )

    private val stopped = AtomicBoolean(true)
    private var socket: SSLSocket? = null
    private var input: DataInputStream? = null
    private var output: DataOutputStream? = null

    suspend fun connect(
        endpoint: InetSocketAddress,
        pinnedPeer: X509Certificate?,
        firstPair: Boolean,
    ): Peer = withContext(Dispatchers.IO) {
        require(capabilitiesHash.size == 32)
        stop()
        stopped.set(false)
        try {
            val tls = PinnedTls.context(identity, pinnedPeer, firstPair)
            val connected = (tls.socketFactory.createSocket() as SSLSocket).apply {
                connect(endpoint, 5_000)
                soTimeout = 5_000
                PinnedTls.configure(this)
                startHandshake()
            }
            socket = connected
            input = DataInputStream(BufferedInputStream(connected.inputStream))
            output = DataOutputStream(BufferedOutputStream(connected.outputStream))

            val peerCertificate = connected.session.peerCertificates.firstOrNull() as? X509Certificate
                ?: error("WABridge peer did not present a certificate")
            val hello = SessionHello(
                DeviceRole.ANDROID,
                SessionHelloCodec.freshNonce(),
                deviceId,
                capabilitiesHash.copyOf(),
                4 * 1024 * 1024,
            )
            send(Envelope(1, 1, 1, 1L, SessionHelloCodec.encode(hello)))
            val peerHello = readHello()
            Peer(peerCertificate, PinnedTls.fingerprint(peerCertificate), peerHello)
        } catch (error: Throwable) {
            stop()
            throw error
        }
    }

    private fun send(envelope: Envelope) {
        val destination = output ?: throw IllegalStateException("WABridge session is not connected")
        destination.write(EnvelopeCodec.encode(envelope))
        destination.flush()
    }

    private fun readHello(): SessionHello {
        val source = input ?: throw IllegalStateException("WABridge session is not connected")
        val header = ByteArray(20)
        source.readFully(header)
        val length = ((header[16].toInt() and 0xff) shl 24) or
            ((header[17].toInt() and 0xff) shl 16) or
            ((header[18].toInt() and 0xff) shl 8) or
            (header[19].toInt() and 0xff)
        if (length <= 0 || length > 64 * 1024) throw ProtocolException("invalid hello frame length")
        val frame = header + ByteArray(length).also(source::readFully)
        val envelope = EnvelopeCodec.decode(frame)
        if (envelope.channel != 1 || envelope.kind != 1) {
            throw ProtocolException("expected SESSION_HELLO")
        }
        val hello = SessionHelloCodec.decode(envelope.payload)
        if (hello.role != DeviceRole.WINDOWS) throw ProtocolException("expected Windows peer")
        return hello
    }

    fun stop() {
        if (!stopped.compareAndSet(false, true)) return
        runCatching { input?.close() }
        runCatching { output?.close() }
        runCatching { socket?.close() }
        input = null
        output = null
        socket = null
    }
}
