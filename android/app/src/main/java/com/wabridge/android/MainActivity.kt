package com.wabridge.android

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.wabridge.android.session.SessionRuntime
import com.wabridge.android.session.WABridgeSessionService
import kotlinx.coroutines.flow.collectAsState

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            WABridgeShell(
                onStart = { startSession() },
                onApprovePairing = { sendServiceAction(WABridgeSessionService.ACTION_APPROVE_PAIRING) },
                onStop = { sendServiceAction(WABridgeSessionService.ACTION_STOP) },
            )
        }
    }

    private fun startSession() {
        ContextCompat.startForegroundService(
            this,
            Intent(this, WABridgeSessionService::class.java).setAction(
                WABridgeSessionService.ACTION_START,
            ),
        )
    }

    private fun sendServiceAction(action: String) {
        startService(Intent(this, WABridgeSessionService::class.java).setAction(action))
    }
}

@Composable
private fun WABridgeShell(
    onStart: () -> Unit,
    onApprovePairing: () -> Unit,
    onStop: () -> Unit,
) {
    val state by SessionRuntime.state.collectAsState()
    val detail by SessionRuntime.detail.collectAsState()
    val pairing by SessionRuntime.pairing.collectAsState()

    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            modifier = Modifier.padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Text("WABridge", style = MaterialTheme.typography.headlineLarge)
            Text("Windows–Android workspace bridge")
            Text("State: $state", style = MaterialTheme.typography.titleMedium)
            Text(detail)
            Button(onClick = onStart) { Text("Find Windows laptop") }
            Button(onClick = onStop) { Text("Stop session") }
            if (pairing != null) {
                val prompt = pairing!!
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
