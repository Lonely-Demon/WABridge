package com.wabridge.android

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.wabridge.android.session.SessionRuntime
import com.wabridge.android.session.SessionState
import com.wabridge.android.session.WABridgeSessionService

class MainActivity : ComponentActivity() {
    private val projectionLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) { result ->
        val data = result.data ?: return@registerForActivityResult
        if (result.resultCode == RESULT_OK) {
            startService(
                Intent(this, WABridgeSessionService::class.java)
                    .setAction(WABridgeSessionService.ACTION_START_AUDIO_CAPTURE)
                    .putExtra(WABridgeSessionService.EXTRA_RESULT_CODE, result.resultCode)
                    .putExtra(WABridgeSessionService.EXTRA_RESULT_DATA, data),
            )
        }
    }

    private val microphonePermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        if (granted) launchProjectionConsent()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            WABridgeShell(
                onStart = { startSession() },
                onManualConnect = { host, port -> startManualSession(host, port) },
                onStartAudioCapture = { requestAudioCapture() },
                onApprovePairing = { sendServiceAction(WABridgeSessionService.ACTION_APPROVE_PAIRING) },
                onStop = { sendServiceAction(WABridgeSessionService.ACTION_STOP) },
            )
        }
    }

    private fun requestAudioCapture() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            microphonePermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
        } else {
            launchProjectionConsent()
        }
    }

    private fun launchProjectionConsent() {
        val manager = getSystemService(MediaProjectionManager::class.java)
        projectionLauncher.launch(manager.createScreenCaptureIntent())
    }

    private fun startSession() {
        startForegroundAction(
            Intent(this, WABridgeSessionService::class.java)
                .setAction(WABridgeSessionService.ACTION_START),
        )
    }

    private fun startManualSession(host: String, port: Int) {
        startForegroundAction(
            Intent(this, WABridgeSessionService::class.java)
                .setAction(WABridgeSessionService.ACTION_START)
                .putExtra(WABridgeSessionService.EXTRA_HOST, host)
                .putExtra(WABridgeSessionService.EXTRA_PORT, port),
        )
    }

    private fun startForegroundAction(intent: Intent) {
        ContextCompat.startForegroundService(this, intent)
    }

    private fun sendServiceAction(action: String) {
        startService(Intent(this, WABridgeSessionService::class.java).setAction(action))
    }
}

@Composable
private fun WABridgeShell(
    onStart: () -> Unit,
    onManualConnect: (String, Int) -> Unit,
    onStartAudioCapture: () -> Unit,
    onApprovePairing: () -> Unit,
    onStop: () -> Unit,
) {
    val state by SessionRuntime.state.collectAsState()
    val detail by SessionRuntime.detail.collectAsState()
    val pairing by SessionRuntime.pairing.collectAsState()
    val pairingSnapshot = pairing
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("51820") }

    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            modifier = Modifier.padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("WABridge", style = MaterialTheme.typography.headlineLarge)
            Text("Windows–Android workspace bridge")
            Text("State: $state", style = MaterialTheme.typography.titleMedium)
            Text(detail)
            Button(onClick = onStart) { Text("Find Windows laptop") }
            OutlinedTextField(
                value = host,
                onValueChange = { host = it.take(253) },
                label = { Text("Windows IP or hostname") },
                singleLine = true,
            )
            OutlinedTextField(
                value = port,
                onValueChange = { port = it.filter(Char::isDigit).take(5) },
                label = { Text("TCP port") },
                singleLine = true,
            )
            Button(
                enabled = host.isNotBlank() && port.toIntOrNull() in 1..65535,
                onClick = { onManualConnect(host.trim(), port.toInt()) },
            ) { Text("Connect manually") }
            Button(
                enabled = state == SessionState.ESTABLISHED,
                onClick = onStartAudioCapture,
            ) { Text("Capture phone audio to Windows") }
            Button(onClick = onStop) { Text("Stop session") }
            if (pairingSnapshot != null) {
                val prompt = pairingSnapshot
                Text("New Windows device requires approval", style = MaterialTheme.typography.titleMedium)
                Text("Device: ${prompt.deviceId}")
                Text("Fingerprint: ${prompt.fingerprint}")
                Button(onClick = onApprovePairing) { Text("Approve pairing") }
            }
            Text(
                "TLS 1.3 and certificate pinning protect the session. " +
                    "Phone Control and MediaProjection remain user-authorized features.",
            )
        }
    }
}
