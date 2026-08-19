package com.wabridge.android.session

enum class SessionState {
    IDLE,
    DISCOVERING,
    CONNECTING,
    TLS_HANDSHAKING,
    IDENTITY_CHECKING,
    PAIRING_REQUIRED,
    ESTABLISHED,
    CLOSING,
    FAILED,
}

enum class SessionEvent {
    BEGIN_DISCOVERY,
    CANDIDATE_FOUND,
    TLS_STARTED,
    TLS_SUCCEEDED,
    IDENTITY_MATCHES,
    PAIRING_NEEDED,
    PAIRING_APPROVED,
    FAILURE,
    STOP,
    CLOSED,
}

class SessionStateMachine {
    var state: SessionState = SessionState.IDLE
        private set
    var stopped: Boolean = false
        private set

    fun apply(event: SessionEvent): Boolean {
        if (event == SessionEvent.STOP) {
            stop()
            return true
        }
        if (event == SessionEvent.CLOSED && state == SessionState.CLOSING) {
            state = SessionState.IDLE
            stopped = true
            return true
        }
        if (stopped && event != SessionEvent.BEGIN_DISCOVERY) return false

        when (state) {
            SessionState.IDLE -> if (event == SessionEvent.BEGIN_DISCOVERY) {
                state = SessionState.DISCOVERING
                stopped = false
            } else return false
            SessionState.DISCOVERING -> if (event == SessionEvent.CANDIDATE_FOUND) {
                state = SessionState.CONNECTING
            } else return false
            SessionState.CONNECTING -> if (event == SessionEvent.TLS_STARTED) {
                state = SessionState.TLS_HANDSHAKING
            } else return false
            SessionState.TLS_HANDSHAKING -> when (event) {
                SessionEvent.TLS_SUCCEEDED -> state = SessionState.IDENTITY_CHECKING
                SessionEvent.FAILURE -> state = SessionState.FAILED
                else -> return false
            }
            SessionState.IDENTITY_CHECKING -> when (event) {
                SessionEvent.IDENTITY_MATCHES -> state = SessionState.ESTABLISHED
                SessionEvent.PAIRING_NEEDED -> state = SessionState.PAIRING_REQUIRED
                SessionEvent.FAILURE -> state = SessionState.FAILED
                else -> return false
            }
            SessionState.PAIRING_REQUIRED -> when (event) {
                SessionEvent.PAIRING_APPROVED -> state = SessionState.ESTABLISHED
                SessionEvent.FAILURE -> state = SessionState.FAILED
                else -> return false
            }
            SessionState.ESTABLISHED -> if (event == SessionEvent.FAILURE) {
                state = SessionState.FAILED
            } else return false
            SessionState.CLOSING, SessionState.FAILED -> return false
        }
        return true
    }

    fun stop() {
        if (stopped) return
        stopped = true
        if (state != SessionState.IDLE) state = SessionState.CLOSING
    }
}
