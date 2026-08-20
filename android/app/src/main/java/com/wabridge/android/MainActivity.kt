package com.wabridge.android

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Audiotrack
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.ContentPaste
import androidx.compose.material.icons.filled.Devices
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.HelpOutline
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.QrCode2
import androidx.compose.material.icons.filled.ScreenShare
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.TouchApp
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.wabridge.android.session.SessionRuntime
import com.wabridge.android.session.SessionState
import com.wabridge.android.session.WABridgeSessionService

private val WABlue = Color(0xFF4B8EFF)
private val WADimBlue = Color(0xFFADC6FF)
private val WACyan = Color(0xFF5DE6FF)
private val WAGold = Color(0xFFFFD60A)
private val WAOrange = Color(0xFFEF6719)
private val WAGreen = Color(0xFF32D74B)
private val WABackground = Color(0xFF131314)
private val WASurface = Color(0xFF1C1C1E)
private val WAStroke = Color(0x2AFFFFFF)
private val WAMuted = Color(0xFFC1C6D7)

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
            WABridgeTheme {
                WABridgeShell(
                    onStart = { startSession() },
                    onManualConnect = { host, port -> startManualSession(host, port) },
                    onStartAudioCapture = { requestAudioCapture() },
                    onApprovePairing = { sendServiceAction(WABridgeSessionService.ACTION_APPROVE_PAIRING) },
                    onStop = { sendServiceAction(WABridgeSessionService.ACTION_STOP) },
                )
            }
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
        startForegroundAction(Intent(this, WABridgeSessionService::class.java).setAction(WABridgeSessionService.ACTION_START))
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

private enum class WABridgeTab(val label: String, val icon: ImageVector) {
    CONNECT("Connect", Icons.Filled.PlayArrow),
    FEATURES("Features", Icons.Filled.Devices),
    SETUP("Setup", Icons.Filled.HelpOutline),
    SETTINGS("Settings", Icons.Filled.Settings),
}

@Composable
private fun WABridgeTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = androidx.compose.material3.darkColorScheme(
            primary = WABlue,
            secondary = WACyan,
            background = WABackground,
            surface = WABackground,
            onBackground = Color.White,
            onSurface = Color.White,
            onSurfaceVariant = WAMuted,
        ),
        content = content,
    )
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
    var tab by remember { mutableStateOf(WABridgeTab.CONNECT) }
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("51820") }
    var manualVisible by remember { mutableStateOf(false) }

    Surface(modifier = Modifier.fillMaxSize(), color = WABackground) {
        Box(modifier = Modifier.fillMaxSize()) {
            when (tab) {
                WABridgeTab.CONNECT -> ConnectDashboard(
                    state = state,
                    detail = detail,
                    pairing = pairing,
                    host = host,
                    port = port,
                    manualVisible = manualVisible,
                    onHostChange = { host = it.take(253) },
                    onPortChange = { port = it.filter(Char::isDigit).take(5) },
                    onStart = onStart,
                    onManualToggle = { manualVisible = !manualVisible },
                    onManualConnect = { onManualConnect(host.trim(), port.toInt()) },
                    onApprovePairing = onApprovePairing,
                    onStartAudioCapture = onStartAudioCapture,
                    onStop = onStop,
                )
                WABridgeTab.FEATURES -> FeaturesScreen(connected = state == SessionState.ESTABLISHED, onStartAudioCapture = onStartAudioCapture)
                WABridgeTab.SETUP -> SetupScreen()
                WABridgeTab.SETTINGS -> SettingsScreen()
            }
            FloatingNavigation(selected = tab, onSelect = { tab = it }, modifier = Modifier.align(Alignment.BottomCenter))
        }
    }
}

@Composable
private fun ConnectDashboard(
    state: SessionState,
    detail: String,
    pairing: com.wabridge.android.session.PairingPrompt?,
    host: String,
    port: String,
    manualVisible: Boolean,
    onHostChange: (String) -> Unit,
    onPortChange: (String) -> Unit,
    onStart: () -> Unit,
    onManualToggle: () -> Unit,
    onManualConnect: () -> Unit,
    onApprovePairing: () -> Unit,
    onStartAudioCapture: () -> Unit,
    onStop: () -> Unit,
) {
    val scroll = rememberScrollState()
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(scroll)
            .padding(horizontal = 20.dp)
            .padding(bottom = 112.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Header(icon = Icons.Filled.Devices, title = "WABridge", subtitle = "Windows–Android workspace bridge")
        StateBadge(state)
        GlassCard(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.padding(20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                IconTile(Icons.Filled.ScreenShare, WABlue, Modifier.size(72.dp))
                Spacer(Modifier.height(16.dp))
                Text(
                    when (state) {
                        SessionState.ESTABLISHED -> "Encrypted session ready"
                        SessionState.PAIRING_REQUIRED -> "Approval needed"
                        SessionState.DISCOVERING, SessionState.CONNECTING, SessionState.TLS_HANDSHAKING, SessionState.IDENTITY_CHECKING -> "Finding your Windows laptop"
                        SessionState.FAILED -> "Connection needs attention"
                        else -> "Connect your Windows laptop"
                    },
                    style = MaterialTheme.typography.headlineSmall.copy(fontWeight = FontWeight.Bold),
                    textAlign = TextAlign.Center,
                )
                Spacer(Modifier.height(8.dp))
                Text(
                    if (detail.isBlank()) "Second Display, Phone Control, files, clipboard, and audio over Wi-Fi." else detail,
                    color = WAMuted,
                    textAlign = TextAlign.Center,
                    lineHeight = 21.sp,
                )
                Spacer(Modifier.height(18.dp))
                GradientButton(
                    text = if (state == SessionState.ESTABLISHED) "Connected to Windows" else "Find Windows laptop",
                    enabled = state != SessionState.ESTABLISHED && state != SessionState.PAIRING_REQUIRED,
                    onClick = onStart,
                    icon = if (state == SessionState.ESTABLISHED) Icons.Filled.CheckCircle else Icons.Filled.Wifi,
                )
                Spacer(Modifier.height(10.dp))
                GlassButton(text = if (manualVisible) "Hide manual connection" else "Connect by IP address", onClick = onManualToggle, icon = Icons.Filled.QrCode2)
                AnimatedVisibility(visible = manualVisible) {
                    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                        OutlinedTextField(
                            value = host,
                            onValueChange = onHostChange,
                            label = { Text("Windows IP or hostname") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                            colors = fieldColors(),
                        )
                        OutlinedTextField(
                            value = port,
                            onValueChange = onPortChange,
                            label = { Text("TCP port") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                            colors = fieldColors(),
                        )
                        GradientButton(
                            text = "Connect manually",
                            enabled = host.isNotBlank() && port.toIntOrNull() in 1..65535,
                            onClick = onManualConnect,
                        )
                    }
                }
            }
        }
        if (pairing != null) {
            PairingCard(pairing, onApprovePairing)
        }
        CapabilityPreview(connected = state == SessionState.ESTABLISHED, onStartAudioCapture = onStartAudioCapture)
        SettingsCard(Icons.Filled.Security, "Encrypted by default", "WABridge uses TLS 1.3, certificate pinning, and explicit first-pair approval.", WABlue)
        GlassButton(text = "Stop session", onClick = onStop, icon = Icons.Filled.Warning, tint = Color(0xFFFF8A80))
    }
}

@Composable
private fun CapabilityPreview(connected: Boolean, onStartAudioCapture: () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        SectionLabel(Icons.Filled.Devices, "Workspace features")
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            CapabilityCard("Second Display", "Virtual screen", Icons.Filled.ScreenShare, WABlue, connected, Modifier.weight(1f))
            CapabilityCard("Phone Control", "Mouse + keyboard", Icons.Filled.TouchApp, WACyan, connected, Modifier.weight(1f))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            CapabilityCard("Files", "Wi-Fi transfer", Icons.Filled.Folder, WAGold, connected, Modifier.weight(1f))
            CapabilityCard("Clipboard", "Stay in sync", Icons.Filled.ContentPaste, WAGreen, connected, Modifier.weight(1f))
        }
        GlassCard(modifier = Modifier.fillMaxWidth(), onClick = if (connected) onStartAudioCapture else null) {
            Row(modifier = Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                IconTile(Icons.Filled.Audiotrack, WACyan)
                Spacer(Modifier.width(12.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text("Phone audio", fontWeight = FontWeight.SemiBold)
                    Text(if (connected) "Tap to request capture permission" else "Available after connection", color = WAMuted, fontSize = 13.sp)
                }
                Icon(Icons.Filled.KeyboardArrowRight, contentDescription = null, tint = WAMuted)
            }
        }
    }
}

@Composable
private fun CapabilityCard(title: String, subtitle: String, icon: ImageVector, tint: Color, connected: Boolean, modifier: Modifier) {
    GlassCard(modifier = modifier) {
        Column(modifier = Modifier.padding(14.dp)) {
            IconTile(icon, tint)
            Spacer(Modifier.height(10.dp))
            Text(title, fontWeight = FontWeight.SemiBold)
            Text(if (connected) subtitle else "Waiting for connection", color = WAMuted, fontSize = 12.sp)
        }
    }
}

@Composable
private fun PairingCard(pairing: com.wabridge.android.session.PairingPrompt, onApprove: () -> Unit) {
    GlassCard(modifier = Modifier.fillMaxWidth(), borderColor = Color(0x66FFD60A)) {
        Column(modifier = Modifier.padding(18.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                IconTile(Icons.Filled.Security, WAGold)
                Spacer(Modifier.width(12.dp))
                Column {
                    Text("New Windows device", fontWeight = FontWeight.Bold)
                    Text("Compare the fingerprint before approving", color = WAMuted, fontSize = 13.sp)
                }
            }
            Spacer(Modifier.height(14.dp))
            Text(pairing.deviceId, fontWeight = FontWeight.SemiBold)
            Text(pairing.fingerprint, color = WAGold, fontSize = 13.sp)
            Spacer(Modifier.height(14.dp))
            GradientButton(text = "Approve pairing", onClick = onApprove, icon = Icons.Filled.CheckCircle)
        }
    }
}

@Composable
private fun FeaturesScreen(connected: Boolean, onStartAudioCapture: () -> Unit) {
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 20.dp).padding(bottom = 112.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Header(Icons.Filled.Devices, "Workspace", "Everything your Windows laptop and Android phone share")
        FeatureLargeCard("Second Display", "Use Android as an additional Windows screen over Wi-Fi.", Icons.Filled.ScreenShare, WABlue, connected)
        FeatureLargeCard("Phone Control", "Move Windows mouse and keyboard input to Android through user-enabled AccessibilityService access.", Icons.Filled.TouchApp, WACyan, connected)
        FeatureLargeCard("Files and Clipboard", "Keep files and copied text moving between devices without a cloud account.", Icons.Filled.Folder, WAGold, connected)
        FeatureLargeCard("Audio routing", "Play Android audio on Windows after explicit MediaProjection consent.", Icons.Filled.Audiotrack, WAGreen, connected, onClick = if (connected) onStartAudioCapture else null)
    }
}

@Composable
private fun FeatureLargeCard(title: String, description: String, icon: ImageVector, tint: Color, connected: Boolean, onClick: (() -> Unit)? = null) {
    GlassCard(modifier = Modifier.fillMaxWidth(), onClick = onClick) {
        Row(modifier = Modifier.padding(18.dp), verticalAlignment = Alignment.Top) {
            IconTile(icon, tint)
            Spacer(Modifier.width(14.dp))
            Column(modifier = Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(title, fontWeight = FontWeight.Bold, fontSize = 18.sp)
                    Spacer(Modifier.width(8.dp))
                    StatusDot(connected, tint)
                }
                Spacer(Modifier.height(6.dp))
                Text(description, color = WAMuted, lineHeight = 20.sp)
                Spacer(Modifier.height(8.dp))
                Text(if (connected) "Ready on this session" else "Connect Windows first", color = if (connected) tint else WAMuted, fontSize = 13.sp)
            }
        }
    }
}

@Composable
private fun SetupScreen() {
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 20.dp).padding(bottom = 112.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Header(Icons.Filled.HelpOutline, "Get connected", "A short Wi-Fi-only setup")
        StepCard(1, "Start WABridge on Windows", "Open the Windows coordinator and start the secure listener on the same Wi-Fi network.", Icons.Filled.Devices, WABlue)
        StepCard(2, "Find or enter the laptop", "Use discovery when available, or enter the Windows IP address and TCP port manually.", Icons.Filled.Wifi, WACyan)
        StepCard(3, "Compare and approve", "On first pairing, compare the device fingerprint on both screens before approving the encrypted session.", Icons.Filled.Security, WAGold)
        StepCard(4, "Choose a workspace feature", "Use Second Display, Phone Control, file transfer, clipboard, or audio as each capability becomes ready.", Icons.Filled.ScreenShare, WAGreen)
    }
}

@Composable
private fun SettingsScreen() {
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 20.dp).padding(bottom = 112.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Header(Icons.Filled.Settings, "Settings", "Security and device preferences")
        SettingsCard(Icons.Filled.Security, "Encrypted by default", "WABridge uses TLS 1.3, certificate pinning, and explicit first-pair approval.", WABlue)
        SettingsCard(Icons.Filled.Wifi, "Wi-Fi only", "No cloud account, Bluetooth, or USB connection is required for the workspace bridge.", WACyan)
        SettingsCard(Icons.Filled.Info, "About WABridge", "Windows and Android companion features are enabled progressively as each platform permission is granted.", WAMuted)
    }
}

@Composable
private fun StepCard(number: Int, title: String, description: String, icon: ImageVector, tint: Color) {
    GlassCard(modifier = Modifier.fillMaxWidth()) {
        Row(modifier = Modifier.padding(16.dp), verticalAlignment = Alignment.Top) {
            Box(modifier = Modifier.size(42.dp).clip(CircleShape).background(tint.copy(alpha = 0.16f)), contentAlignment = Alignment.Center) {
                Text(number.toString(), color = tint, fontWeight = FontWeight.Bold)
            }
            Spacer(Modifier.width(12.dp))
            Column(modifier = Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(icon, contentDescription = null, tint = tint, modifier = Modifier.size(20.dp))
                    Spacer(Modifier.width(8.dp))
                    Text(title, fontWeight = FontWeight.Bold)
                }
                Spacer(Modifier.height(6.dp))
                Text(description, color = WAMuted, lineHeight = 20.sp)
            }
        }
    }
}

@Composable
private fun SettingsCard(icon: ImageVector, title: String, description: String, tint: Color) {
    GlassCard(modifier = Modifier.fillMaxWidth()) {
        Row(modifier = Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            IconTile(icon, tint)
            Spacer(Modifier.width(12.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(title, fontWeight = FontWeight.SemiBold)
                Spacer(Modifier.height(4.dp))
                Text(description, color = WAMuted, fontSize = 13.sp, lineHeight = 19.sp)
            }
        }
    }
}

@Composable
private fun Header(icon: ImageVector, title: String, subtitle: String) {
    Row(modifier = Modifier.fillMaxWidth().padding(top = 18.dp), verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, contentDescription = null, tint = WABlue, modifier = Modifier.size(28.dp))
        Spacer(Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.Bold))
            Text(subtitle, color = WAMuted, fontSize = 13.sp)
        }
    }
}

@Composable
private fun StateBadge(state: SessionState) {
    val (text, tint) = when (state) {
        SessionState.ESTABLISHED -> "CONNECTED" to WAGreen
        SessionState.PAIRING_REQUIRED -> "APPROVAL REQUIRED" to WAGold
        SessionState.FAILED -> "NEEDS ATTENTION" to WAOrange
        SessionState.DISCOVERING, SessionState.CONNECTING, SessionState.TLS_HANDSHAKING, SessionState.IDENTITY_CHECKING -> "CONNECTING" to WABlue
        else -> "READY TO CONNECT" to WAMuted
    }
    Row(
        modifier = Modifier.clip(CircleShape).background(tint.copy(alpha = 0.14f)).padding(horizontal = 11.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        StatusDot(state == SessionState.ESTABLISHED, tint)
        Spacer(Modifier.width(7.dp))
        Text(text, color = tint, fontSize = 11.sp, fontWeight = FontWeight.Bold, letterSpacing = 0.8.sp)
    }
}

@Composable
private fun StatusDot(healthy: Boolean, tint: Color) {
    Box(modifier = Modifier.size(8.dp).clip(CircleShape).background(if (healthy) WAGreen else tint))
}

@Composable
private fun SectionLabel(icon: ImageVector, text: String) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(icon, contentDescription = null, tint = WAMuted, modifier = Modifier.size(15.dp))
        Spacer(Modifier.width(6.dp))
        Text(text.uppercase(), color = WAMuted, fontSize = 11.sp, fontWeight = FontWeight.Bold, letterSpacing = 1.sp)
    }
}

@Composable
private fun FloatingNavigation(selected: WABridgeTab, onSelect: (WABridgeTab) -> Unit, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .padding(horizontal = 20.dp, vertical = 16.dp)
            .shadow(18.dp, RoundedCornerShape(28.dp), spotColor = Color.Black.copy(alpha = 0.6f))
            .clip(RoundedCornerShape(28.dp))
            .background(Color(0xE61C1C1E))
            .border(BorderStroke(1.dp, WAStroke), RoundedCornerShape(28.dp))
            .padding(6.dp),
        horizontalArrangement = Arrangement.spacedBy(3.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        WABridgeTab.entries.forEach { tab ->
            val active = tab == selected
            Row(
                modifier = Modifier
                    .clip(CircleShape)
                    .background(if (active) WABlue.copy(alpha = 0.17f) else Color.Transparent)
                    .clickable { onSelect(tab) }
                    .padding(horizontal = if (active) 12.dp else 10.dp, vertical = 9.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(tab.icon, contentDescription = tab.label, tint = if (active) WABlue else WAMuted, modifier = Modifier.size(19.dp))
                if (active) {
                    Spacer(Modifier.width(5.dp))
                    Text(tab.label, color = WABlue, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                }
            }
        }
    }
}

@Composable
private fun GlassCard(modifier: Modifier = Modifier, borderColor: Color = WAStroke, onClick: (() -> Unit)? = null, content: @Composable () -> Unit) {
    val shape = RoundedCornerShape(20.dp)
    Column(
        modifier = modifier
            .shadow(14.dp, shape, spotColor = Color.Black.copy(alpha = 0.34f))
            .clip(shape)
            .background(Color(0x0FFFFFFF))
            .border(BorderStroke(1.dp, borderColor), shape)
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier),
        content = { content() },
    )
}

@Composable
private fun IconTile(icon: ImageVector, tint: Color, modifier: Modifier = Modifier) {
    Box(modifier = modifier.size(48.dp).clip(RoundedCornerShape(13.dp)).background(tint.copy(alpha = 0.15f)), contentAlignment = Alignment.Center) {
        Icon(icon, contentDescription = null, tint = tint, modifier = Modifier.size(25.dp))
    }
}

@Composable
private fun GradientButton(text: String, onClick: () -> Unit, enabled: Boolean = true, icon: ImageVector? = null, modifier: Modifier = Modifier) {
    val shape = RoundedCornerShape(14.dp)
    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(46.dp)
            .shadow(if (enabled) 10.dp else 0.dp, shape, spotColor = WABlue.copy(alpha = 0.5f))
            .clip(shape)
            .background(if (enabled) Brush.horizontalGradient(listOf(Color(0xFF007AFF), WACyan)) else Brush.linearGradient(listOf(Color(0xFF303238), Color(0xFF292A2E))))
            .clickable(enabled = enabled, onClick = onClick),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) {
            Icon(icon, contentDescription = null, tint = if (enabled) Color.White else WAMuted, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
        }
        Text(text, color = if (enabled) Color.White else WAMuted, fontWeight = FontWeight.SemiBold, fontSize = 15.sp)
    }
}

@Composable
private fun GlassButton(text: String, onClick: () -> Unit, icon: ImageVector? = null, tint: Color = Color.White) {
    val shape = RoundedCornerShape(14.dp)
    Row(
        modifier = Modifier.fillMaxWidth().height(46.dp).clip(shape).background(Color(0x0FFFFFFF)).border(BorderStroke(1.dp, WAStroke), shape).clickable(onClick = onClick),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) {
            Icon(icon, contentDescription = null, tint = tint, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
        }
        Text(text, color = tint, fontWeight = FontWeight.SemiBold, fontSize = 15.sp)
    }
}

@Composable
private fun fieldColors() = OutlinedTextFieldDefaults.colors(
    focusedTextColor = Color.White,
    unfocusedTextColor = Color.White,
    focusedBorderColor = WABlue,
    unfocusedBorderColor = WAStroke,
    focusedLabelColor = WABlue,
    unfocusedLabelColor = WAMuted,
    cursorColor = WABlue,
)
