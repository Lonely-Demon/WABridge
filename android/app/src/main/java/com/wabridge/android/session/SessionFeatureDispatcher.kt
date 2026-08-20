package com.wabridge.android.session

import com.wabridge.android.audio.AudioCodecWire
import com.wabridge.android.audio.AudioFrame
import com.wabridge.android.clipboard.ClipboardCodec
import com.wabridge.android.clipboard.ClipboardUpdate
import com.wabridge.android.file.FileChunk
import com.wabridge.android.file.FileCodec
import com.wabridge.android.file.FileOffer
import com.wabridge.android.input.InputEvent
import com.wabridge.android.input.InputEventCodec
import com.wabridge.android.protocol.Envelope
import com.wabridge.android.protocol.ProtocolException

class SessionFeatureDispatcher {
    var onFileOffer: ((FileOffer) -> Boolean)? = null
    var onFileChunk: ((FileChunk) -> Boolean)? = null
    var onClipboardUpdate: ((ClipboardUpdate) -> Boolean)? = null
    var onAudioFrame: ((AudioFrame) -> Boolean)? = null
    var onInputEvent: ((InputEvent) -> Boolean)? = null

    fun dispatch(envelope: Envelope): Boolean = try {
        when (envelope.channel) {
            1 -> if (envelope.kind == KIND_INPUT_EVENT) {
                onInputEvent?.invoke(InputEventCodec.decode(envelope.payload)) == true
            } else {
                false
            }
            3 -> when (envelope.kind) {
                KIND_FILE_OFFER -> onFileOffer?.invoke(FileCodec.decodeOffer(envelope.payload)) == true
                KIND_FILE_CHUNK -> onFileChunk?.invoke(FileCodec.decodeChunk(envelope.payload)) == true
                else -> false
            }
            4 -> if (envelope.kind == KIND_CLIPBOARD_UPDATE) {
                onClipboardUpdate?.invoke(ClipboardCodec.decode(envelope.payload)) == true
            } else {
                false
            }
            5 -> if (envelope.kind == KIND_AUDIO_FRAME) {
                onAudioFrame?.invoke(AudioCodecWire.decode(envelope.payload)) == true
            } else {
                false
            }
            else -> false
        }
    } catch (_: ProtocolException) {
        false
    } catch (_: IllegalArgumentException) {
        false
    }

    companion object {
        const val KIND_FILE_OFFER = 0x0001
        const val KIND_FILE_CHUNK = 0x0002
        const val KIND_CLIPBOARD_UPDATE = 0x0001
        const val KIND_AUDIO_FRAME = 0x0001
        const val KIND_INPUT_EVENT = 0x0102
    }
}
