package com.wabridge.android.session

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.net.nsd.NsdServiceInfo
import androidx.core.app.NotificationCompat
import com.wabridge.android.MainActivity
import com.wabridge.android.discovery.NsdDiscovery
import com.wabridge.android.pairing.AndroidIdentityStore
import com.wabridge.android.audio.AudioPlayback
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
import java.net.InetSocketAddress
import java.security.MessageDigest
import java.util.concurrent.atomic.AtomicBoolean

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

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        identity = AndroidIdentityStore(this)
        identity.ensureIdentity()
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
                startForeground(NOTIFICATION_ID, notification("Starting WABridge session"))
                begin(intent)
            }
            ACTION_APPROVE_PAIRING -> approvePairing()
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
        if (!host.isNullOrBlank() && port in 1..65535) {
            connectTo(InetSocketAddress(host, port), deviceId)
            return
        }

        discovery?.stop()
        discovery = NsdDiscovery(
            context = this,
            fingerprint = identity::fingerprint,
            onCandidate = { info -> onCandidate(info) },
            onError = { fail(it) },
        ).also { it.startBrowsing() }
        SessionRuntime.update(SessionState.DISCOVERING, "Searching for WABridge on this Wi-Fi")
    }

    private fun onCandidate(info: NsdServiceInfo) {
        if (destroyed.get() || connectJob?.isActive == true) return
        val host = info.host ?: return fail("Windows coordinator did not provide an address")
        val deviceId = info.attributes[TXT_DEVICE_ID]
            ?.toString(Charsets.UTF_8)
            ?.takeIf { it.isNotBlank() }
        sessions.apply(SessionEvent.CANDIDATE_FOUND)
        connectTo(InetSocketAddress(host, info.port), deviceId)
    }

    private fun connectTo(endpoint: InetSocketAddress, advertisedDeviceId: String?) {
        if (connectJob?.isActive == true || destroyed.get()) return
        connectJob = scope.launch {
            val expectedFingerprint = advertisedDeviceId?.let(identity::peerFingerprint)
            val firstPair = expectedFingerprint == null
            val session = TlsSessionClient(
                identity = identity,
                deviceId = "android-${identity.fingerprint().replace(":", "").take(16)}",
                capabilitiesHash = MessageDigest.getInstance("SHA-256")
                    .digest("android-session-v1".toByteArray(Charsets.US_ASCII)),
            )
            client = session
            try {
                sessions.apply(SessionEvent.TLS_STARTED)
                SessionRuntime.update(SessionState.TLS_HANDSHAKING, "Establishing a pinned TLS 1.3 session")
                val peer = session.connect(endpoint, expectedFingerprint, firstPair)
                sessions.apply(SessionEvent.TLS_SUCCEEDED)
                if (advertisedDeviceId != null && advertisedDeviceId != peer.hello.deviceId) {
                    throw SecurityException("Windows device identity does not match its discovery record")
                }
                if (!firstPair && expectedFingerprint == peer.fingerprint) {
                    sessions.apply(SessionEvent.IDENTITY_MATCHES)
                    SessionRuntime.update(SessionState.ESTABLISHED, "Securely connected to ${peer.hello.deviceId}")
                    startReceiveLoop(session)
                } else {
                    pendingPeer = peer
                    sessions.apply(SessionEvent.PAIRING_NEEDED)
                    SessionRuntime.requirePairing(
                        PairingPrompt(peer.hello.deviceId, peer.fingerprint),
                    )
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                fail("Session failed: ${error.message ?: error.javaClass.simpleName}")
                session.stop()
            }
        }
    }

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
        sessions.apply(SessionEvent.FAILURE)
        SessionRuntime.update(SessionState.FAILED, detail)
        updateNotification(detail)
    }

    private fun stopSession() {
        if (!destroyed.compareAndSet(false, true)) return
        connectJob?.cancel()
        connectJob = null
        receiveJob?.cancel()
        receiveJob = null
        discovery?.stop()
        discovery = null
        client?.stop()
        client = null
        audioPlayback.stop()
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
        const val EXTRA_HOST = "com.wabridge.android.extra.HOST"
        const val EXTRA_PORT = "com.wabridge.android.extra.PORT"
        const val EXTRA_DEVICE_ID = "com.wabridge.android.extra.DEVICE_ID"
        private const val CHANNEL_ID = "wabridge-session"
        private const val NOTIFICATION_ID = 1001
        private const val TXT_DEVICE_ID = "device_id"
    }
}
