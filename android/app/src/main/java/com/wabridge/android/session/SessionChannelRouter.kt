package com.wabridge.android.session

import com.wabridge.android.protocol.Envelope
import com.wabridge.android.protocol.ProtocolException

class SessionChannelRouter {
    private val handlers = mutableMapOf<Int, (Envelope) -> Boolean>()

    @Synchronized
    fun setHandler(channel: Int, handler: ((Envelope) -> Boolean)?) {
        if (channel !in 1..5 || handler == null) throw ProtocolException("invalid session-router handler")
        handlers[channel] = handler
    }

    @Synchronized
    fun dispatch(envelope: Envelope): Boolean {
        if (envelope.channel !in 1..5) return false
        return handlers[envelope.channel]?.invoke(envelope) == true
    }
}
