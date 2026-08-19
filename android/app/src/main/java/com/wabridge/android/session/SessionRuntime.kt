package com.wabridge.android.session

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/** UI-safe snapshot exposed by the foreground session service. */
data class PairingPrompt(
    val deviceId: String,
    val fingerprint: String,
)

object SessionRuntime {
    private val _state = MutableStateFlow(SessionState.IDLE)
    private val _detail = MutableStateFlow("Not connected")
    private val _pairing = MutableStateFlow<PairingPrompt?>(null)

    val state: StateFlow<SessionState> = _state.asStateFlow()
    val detail: StateFlow<String> = _detail.asStateFlow()
    val pairing: StateFlow<PairingPrompt?> = _pairing.asStateFlow()

    fun update(state: SessionState, detail: String) {
        _state.value = state
        _detail.value = detail
    }

    fun requirePairing(prompt: PairingPrompt) {
        _pairing.value = prompt
        update(SessionState.PAIRING_REQUIRED, "Verify the Windows device fingerprint before approving")
    }

    fun clearPairing() {
        _pairing.value = null
    }

    fun reset() {
        _pairing.value = null
        update(SessionState.IDLE, "Not connected")
    }
}
