package com.wabridge.android.session

import com.wabridge.android.protocol.Envelope
import com.wabridge.android.protocol.ProtocolException
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertThrows
import org.junit.Test

class SessionChannelRouterTest {
    @Test
    fun dispatchesOnlyToRegisteredChannels() {
        val router = SessionChannelRouter()
        var handled = false
        router.setHandler(3) {
            handled = it.kind == 7 && it.payload.contentEquals(byteArrayOf(1, 2, 3))
            handled
        }
        assertTrue(router.dispatch(Envelope(3, 7, 0, 9, byteArrayOf(1, 2, 3))))
        assertTrue(handled)
        assertFalse(router.dispatch(Envelope(4, 1, 0, 1, byteArrayOf(9))))
    }

    @Test
    fun rejectsInvalidHandlersAndChannels() {
        val router = SessionChannelRouter()
        assertThrows(ProtocolException::class.java) { router.setHandler(6) { true } }
        assertThrows(ProtocolException::class.java) { router.setHandler(3, null) }
        assertFalse(router.dispatch(Envelope(0, 1, 0, 1, byteArrayOf(9))))
    }
}
