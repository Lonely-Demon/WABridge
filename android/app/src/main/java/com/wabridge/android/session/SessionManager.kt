package com.wabridge.android.session

import java.util.concurrent.atomic.AtomicBoolean

class SessionManager(
    private val onStateChanged: (SessionState) -> Unit,
) {
    private val lock = Any()
    private val stopped = AtomicBoolean(false)
    private val machine = SessionStateMachine()

    fun state(): SessionState = synchronized(lock) { machine.state }

    fun apply(event: SessionEvent): Boolean = synchronized(lock) {
        if (stopped.get() && event != SessionEvent.BEGIN_DISCOVERY) return false
        val accepted = machine.apply(event)
        if (accepted) onStateChanged(machine.state)
        accepted
    }

    fun stop() {
        if (!stopped.compareAndSet(false, true)) return
        synchronized(lock) {
            machine.stop()
            onStateChanged(machine.state)
        }
    }
}
