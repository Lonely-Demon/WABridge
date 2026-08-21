package com.wabridge.android.session

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.net.nsd.NsdServiceInfo
import androidx.core.app.NotificationCompat
import com.wabridge.android.MainActivity
import com.wabridge.android.discovery.NsdDiscovery
import com.wabridge.android.pairing.AndroidIdentityStore
import com.wabridge.android.audio.AudioPlayback
import com.wabridge.android.audio.AudioPlaybackCapture
import com.wabridge.android.audio.AudioCodecWire
import com.wabridge.android.input.AccessibilityInputBridge
import com.wabridge.android.protocol.Envelope
import com.wabridge.android.protocol.SessionHelloCodec
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.net.ConnectException
import java.net.InetSocketAddress
import java.net.SocketTimeoutException
import java.net.UnknownHostException
import javax.net.ssl.SSLHandshakeException
import java.security.MessageDigest
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

/**
 * Owns the long-lived Wi-Fi session independently of the Compose activity.
 * No first-pair identity is trusted until the user explicitly approves it.
 */
class WABridgeSessionService : Service() {
    private val destroyed = AtomicBoolean(false)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private lateinit var identity: AndroidIdentityStore
    private lateinit var sessions: SessionManager
    private var discovery: NsdDiscovery? = null
    private var client: TlsSessionClient? = null
    private var connectJob: Job? = null
    private var receiveJob: Job? = null
    private var pendingPeer: TlsSessionClient.Peer? = null
    private val router = SessionChannelRouter()
    private val featureDispatcher = SessionFeatureDispatcher()
    private val audioPlayback = AudioPlayback()
    private lateinit var audioCapture: AudioPlaybackCapture
    private val requestIds = AtomicLong(100)

    override fun onCreate() {
        super.onCreate()
        SessionLog.info("WABridge session service created")
        createNotificationChannel()
        identity = AndroidIdentityStore(this)
        identity.ensureIdentity()
        audioCapture = AudioPlaybackCapture(this)
        featureDispatcher.onInputEvent = { event -> AccessibilityInputBridge.dispatch(event) }
        featureDispatcher.onAudioFrame = { frame -> audioPlayback.play(frame) }
        sessions = SessionManager { state ->
            SessionRuntime.update(state, state.detail())
            updateNotification(state.detail())
        }
        updateNotification("Ready for a Windows coordinator")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> {
                runCatching {
                    val host = intent.getStringExtra(EXTRA_HOST)
                    val port = intent.getIntExtra(EXTRA_PORT, 0)
                    SessionLog.info(if (host.isNullOrBlank()) "Starting Wi-Fi discovery" else "Starting manual connection to $host:$port")
                    promoteSessionForeground("Starting WABridge session")
                    begin(intent)
                }.onFailure { error ->
                    SessionLog.error("Session service start failed: ${error.message ?: error.javaClass.simpleName}")
                    fail("Unable to start WABridge session: ${error.message ?: error.javaClass.simpleName}")
                }
            }
            ACTION_APPROVE_PAIRING -> approvePairing()
            ACTION_START_AUDIO_CAPTURE -> startAudioCapture(intent)
            ACTION_STOP -> stopSession()
        }
        return START_NOT_STICKY
    }

    private fun begin(intent: Intent) {
        if (destroyed.get() || connectJob?.isActive == true) return
        pendingPeer = null
        SessionRuntime.clearPairing()
        sessions.apply(SessionEvent.BEGIN_DISCOVERY)

        val host = intent.getStringExtra(EXTRA_HOST)
        val port = intent.getIntExtra(EXTRA_PORT, 0)
        val deviceId = intent.getStringExtra(EXTRA_DEVICE_ID)
        val expectedFingerprint = intent.getStringExtra(EXTRA_EXPECTED_FINGERPRINT)
        if (!host.isNullOrBlank() && port in 1..65535) {
            SessionLog.info("Manual endpoint accepted: $host:$port")
            connectTo(InetSocketAddress(host, port), deviceId, expectedFingerprint)
            return
        }

        SessionLog.info("Browsing for _wabridge._tcp on local Wi-Fi")
        discovery?.stop()
        discovery = NsdDiscovery(
            context = this,
            fingerprint = identity::fingerprint,
            onCandidate = { info -> onCandidate(info) },
            onError = { SessionLog.warn(it); fail(it) },
        ).also { it.startBrowsing() }
        SessionRuntime.update(SessionState.DISCOVERING, "Searching for WABridge on this Wi-Fi")
    }

    private fun onCandidate(info: NsdServiceInfo) {
        if (destroyed.get() || connectJob?.isActive == true) return
        val host = info.host ?: return fail("Windows coordinator did not provide an address")
        SessionLog.info("Windows advertisement resolved to ${host.hostAddress}:${info.port}")
        val deviceId = info.attributes[TXT_DEVICE_ID]
            ?.toString(Charsets.UTF_8)
            ?.takeIf { it.isNotBlank() }
        sessions.apply(SessionEvent.CANDIDATE_FOUND)
        connectTo(InetSocketAddress(host, info.port), deviceId, null)
    }

    private fun connectTo(endpoint: InetSocketAddress, advertisedDeviceId: String?, suppliedFingerprint: String?) {
        if (connectJob?.isActive == true || destroyed.get()) return
        connectJob = scope.launch {
            val expectedFingerprint = suppliedFingerprint ?: advertisedDeviceId?.let(identity::peerFingerprint)
            val qrBootstrap = suppliedFingerprint != null
            val firstPair = expectedFingerprint == null
            val session = TlsSessionClient(
                identity = identity,
                deviceId = "android-${identity.fingerprint().replace(":", "").take(16)}",
                capabilitiesHash = MessageDigest.getInstance("SHA-256")
                    .digest("android-session-v1".toByteArray(Charsets.US_ASCII)),
            )
            client = session
            try {
                SessionLog.info("Opening TCP connection to ${endpoint.hostString}:${endpoint.port}")
                sessions.apply(SessionEvent.TLS_STARTED)
                SessionRuntime.update(SessionState.TLS_HANDSHAKING, "Establishing a pinned TLS 1.3 session")
                val peer = session.connect(endpoint, expectedFingerprint, firstPair)
                SessionLog.info("TLS 1.3 handshake completed with ${peer.hello.deviceId}")
                sessions.apply(SessionEvent.TLS_SUCCEEDED)
                if (advertisedDeviceId != null && advertisedDeviceId != peer.hello.deviceId) {
                    throw SecurityException("Windows device identity does not match its discovery record")
                }
                if (!qrBootstrap && !firstPair && expectedFingerprint == peer.fingerprint) {
                    sessions.apply(SessionEvent.IDENTITY_MATCHES)
                    SessionRuntime.update(SessionState.ESTABLISHED, "Securely connected to ${peer.hello.deviceId}")
                    startReceiveLoop(session)
                } else {
                    SessionLog.warn(if (qrBootstrap) "QR endpoint verified; explicit first-pair approval required" else "First-pair approval required for ${peer.hello.deviceId}")
                    pendingPeer = peer
                    sessions.apply(SessionEvent.PAIRING_NEEDED)
                    SessionRuntime.requirePairing(
                        PairingPrompt(peer.hello.deviceId, peer.fingerprint),
                    )
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                val detail = connectionFailure(endpoint, error)
                SessionLog.error(detail)
                fail(detail)
                session.stop()
            }
        }
    }

    private fun connectionFailure(endpoint: InetSocketAddress, error: Throwable): String {
        val address = "${endpoint.hostString}:${endpoint.port}"
        return when (error) {
            is SocketTimeoutException -> "Windows at $address did not respond within 5 seconds; check that WABridge is running and Windows Firewall allows TCP ${endpoint.port}"
            is ConnectException -> "Windows at $address refused or blocked the connection; start WABridge and allow TCP ${endpoint.port} through Windows Firewall"
            is UnknownHostException -> "Windows host $address could not be resolved; use the physical Wi-Fi IPv4 address"
            is SSLHandshakeException -> "TCP reached Windows at $address, but TLS verification failed: ${error.message ?: "certificate or protocol mismatch"}"
            is SecurityException -> "Windows identity verification failed at $address: ${error.message ?: "pairing or certificate mismatch"}"
            else -> "Session to Windows at $address failed: ${error.message ?: error.javaClass.simpleName}"
        }
    }

    private fun startAudioCapture(intent: Intent) {
        if (sessions.state() != SessionState.ESTABLISHED) return
        val resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, 0)
        val resultData = parcelableIntent(intent, EXTRA_RESULT_DATA) ?: return
        val session = client ?: return
        val promoted = runCatching {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                startForeground(
                    NOTIFICATION_ID,
                    notification("Capturing phone audio with your permission"),
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE or
                        ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION,
                )
            } else {
                startForeground(NOTIFICATION_ID, notification("Capturing phone audio with your permission"))
            }
            true
        }.getOrElse {
            fail("Unable to start audio permission service: ${it.message ?: it.javaClass.simpleName}")
            false
        }
        if (!promoted) return
        val started = runCatching {
            audioCapture.start(resultCode, resultData) { frame ->
            if (!destroyed.get()) {
                session.sendEnvelope(
                    Envelope(5, SessionFeatureDispatcher.KIND_AUDIO_FRAME, 0, requestIds.incrementAndGet(), AudioCodecWire.encode(frame)),
                )
            }
            }
        }.getOrElse {
            fail("Unable to start phone audio capture: ${it.message ?: it.javaClass.simpleName}")
            false
        }
        if (!started) fail("Phone audio capture could not be initialized on this device")
    }

    private fun promoteSessionForeground(detail: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(
                NOTIFICATION_ID,
                notification(detail),
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE,
            )
        } else {
            startForeground(NOTIFICATION_ID, notification(detail))
        }
    }

    private fun parcelableIntent(intent: Intent, key: String): Intent? =
        if (Build.VERSION.SDK_INT >= 33) intent.getParcelableExtra(key, Intent::class.java)
        else @Suppress("DEPRECATION") intent.getParcelableExtra(key)

    private fun approvePairing() {
        val peer = pendingPeer ?: return
        if (sessions.state() != SessionState.PAIRING_REQUIRED) return
        identity.rememberPeer(peer.hello.deviceId, peer.fingerprint)
        pendingPeer = null
        SessionRuntime.clearPairing()
        sessions.apply(SessionEvent.PAIRING_APPROVED)
        SessionRuntime.update(SessionState.ESTABLISHED, "Securely connected to ${peer.hello.deviceId}")
        client?.let(::startReceiveLoop)
    }

    private fun startReceiveLoop(session: TlsSessionClient) {
        if (receiveJob?.isActive == true) return
        receiveJob = scope.launch(Dispatchers.IO) {
            try {
                while (!destroyed.get()) {
                    val envelope = session.readEnvelope()
                    if (envelope.channel == 1 && envelope.kind == 0x0005) {
                        session.sendEnvelope(Envelope(1, 0x0006, 0, envelope.requestId, byteArrayOf(1)))
                    } else if (envelope.channel == 1 && envelope.kind == 0x0007) {
                        break
                    } else if (!router.dispatch(envelope) && !featureDispatcher.dispatch(envelope)) {
                        throw IllegalStateException("No handler for authenticated feature frame ${envelope.channel}/${envelope.kind}")
                    }
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                if (!destroyed.get()) fail("Session receive failed: ${error.message ?: error.javaClass.simpleName}")
            }
        }
    }

    private fun fail(detail: String) {
        if (destroyed.get()) return
        SessionLog.error(detail)
        sessions.apply(SessionEvent.FAILURE)
        SessionRuntime.update(SessionState.FAILED, detail)
        updateNotification(detail)
    }

    private fun stopSession() {
        if (!destroyed.compareAndSet(false, true)) return
        SessionLog.info("Stopping WABridge session")
        connectJob?.cancel()
        connectJob = null
        receiveJob?.cancel()
        receiveJob = null
        discovery?.stop()
        discovery = null
        client?.stop()
        client = null
        audioPlayback.stop()
        audioCapture.stop()
        pendingPeer = null
        sessions.stop()
        SessionRuntime.reset()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    override fun onDestroy() {
        stopSession()
        scope.cancel()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun updateNotification(detail: String) {
        if (!destroyed.get()) {
            getSystemService(NotificationManager::class.java)
                .notify(NOTIFICATION_ID, notification(detail))
        }
    }

    private fun notification(detail: String): Notification = NotificationCompat.Builder(this, CHANNEL_ID)
        .setSmallIcon(android.R.drawable.stat_sys_upload)
        .setContentTitle("WABridge")
        .setContentText(detail.take(120))
        .setOngoing(true)
        .setContentIntent(android.app.PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            android.app.PendingIntent.FLAG_UPDATE_CURRENT or
                (if (Build.VERSION.SDK_INT >= 23) android.app.PendingIntent.FLAG_IMMUTABLE else 0),
        ))
        .build()

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            getSystemService(NotificationManager::class.java).createNotificationChannel(
                NotificationChannel(
                    CHANNEL_ID,
                    "WABridge session",
                    NotificationManager.IMPORTANCE_LOW,
                ),
            )
        }
    }

    private fun SessionState.detail(): String = when (this) {
        SessionState.IDLE -> "Not connected"
        SessionState.DISCOVERING -> "Searching for WABridge on this Wi-Fi"
        SessionState.CONNECTING -> "Connecting to Windows coordinator"
        SessionState.TLS_HANDSHAKING -> "Establishing a pinned TLS 1.3 session"
        SessionState.IDENTITY_CHECKING -> "Checking Windows device identity"
        SessionState.PAIRING_REQUIRED -> "Pairing approval required"
        SessionState.ESTABLISHED -> "Secure session established"
        SessionState.CLOSING -> "Closing session"
        SessionState.FAILED -> "Session failed"
    }

    companion object {
        const val ACTION_START = "com.wabridge.android.action.START"
        const val ACTION_APPROVE_PAIRING = "com.wabridge.android.action.APPROVE_PAIRING"
        const val ACTION_STOP = "com.wabridge.android.action.STOP"
        const val ACTION_START_AUDIO_CAPTURE = "com.wabridge.android.action.START_AUDIO_CAPTURE"
        const val EXTRA_HOST = "com.wabridge.android.extra.HOST"
        const val EXTRA_PORT = "com.wabridge.android.extra.PORT"
        const val EXTRA_DEVICE_ID = "com.wabridge.android.extra.DEVICE_ID"
        const val EXTRA_EXPECTED_FINGERPRINT = "com.wabridge.android.extra.EXPECTED_FINGERPRINT"
        const val EXTRA_RESULT_CODE = "com.wabridge.android.extra.RESULT_CODE"
        const val EXTRA_RESULT_DATA = "com.wabridge.android.extra.RESULT_DATA"
        private const val CHANNEL_ID = "wabridge-session"
        private const val NOTIFICATION_ID = 1001
        private const val TXT_DEVICE_ID = "device_id"
    }
}
