package com.wabridge.android.session

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SessionStateTest {
    @Test
    fun pairingAndRepeatedStop() {
        val machine = SessionStateMachine()
        assertFalse(machine.apply(SessionEvent.CANDIDATE_FOUND))
        assertTrue(machine.apply(SessionEvent.BEGIN_DISCOVERY))
        assertTrue(machine.apply(SessionEvent.CANDIDATE_FOUND))
        assertTrue(machine.apply(SessionEvent.TLS_STARTED))
        assertTrue(machine.apply(SessionEvent.TLS_SUCCEEDED))
        assertTrue(machine.apply(SessionEvent.PAIRING_NEEDED))
        assertTrue(machine.apply(SessionEvent.PAIRING_APPROVED))
        assertEquals(SessionState.ESTABLISHED, machine.state)

        machine.stop()
        machine.stop()
        assertEquals(SessionState.CLOSING, machine.state)
        assertTrue(machine.apply(SessionEvent.CLOSED))
        assertEquals(SessionState.IDLE, machine.state)
        assertTrue(machine.stopped)
    }
}
