package com.wabridge.android

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
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { WABridgeShell() }
    }
}

@Composable
private fun WABridgeShell() {
    var status by remember { mutableStateOf("Not connected") }
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            modifier = Modifier.padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Text("WABridge", style = MaterialTheme.typography.headlineLarge)
            Text("Windows–Android workspace bridge")
            Text(status, style = MaterialTheme.typography.titleMedium)
            Button(onClick = { status = "Searching for WABridge on this Wi-Fi" }) {
                Text("Find Windows laptop")
            }
            Button(onClick = { status = "Pairing requires matching the code shown on both devices" }) {
                Text("Pair a device")
            }
            Text("Display, Phone Control, files, clipboard, and audio remain disabled until a secure session is established.")
        }
    }
}
