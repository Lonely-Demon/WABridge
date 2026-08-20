package com.wabridge.android.session

import com.wabridge.android.audio.AudioCodec
import com.wabridge.android.audio.AudioCodecWire
import com.wabridge.android.audio.AudioFrame
import com.wabridge.android.clipboard.ClipboardCodec
import com.wabridge.android.clipboard.ClipboardUpdate
import com.wabridge.android.file.FileCodec
import com.wabridge.android.file.FileOffer
import com.wabridge.android.input.InputEvent
import com.wabridge.android.input.InputEventCodec
import com.wabridge.android.input.InputEventType
import com.wabridge.android.display.DisplayCommand
import com.wabridge.android.display.DisplayCommandCodec
import com.wabridge.android.display.DisplayMode
import com.wabridge.android.protocol.Envelope
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SessionFeatureDispatcherTest {
    @Test
    fun decodesTypedFeatureFrames() {
        val dispatcher = SessionFeatureDispatcher()
        var fileSeen = false
        var clipboardSeen = false
        var audioSeen = false
        var inputSeen = false
        var displaySeen = false
        dispatcher.onFileOffer = { fileSeen = it.displayName == "note.txt"; fileSeen }
        dispatcher.onClipboardUpdate = { clipboardSeen = it.text == "hello"; clipboardSeen }
        dispatcher.onAudioFrame = { audioSeen = it.data.size == 4; audioSeen }
        dispatcher.onInputEvent = { inputSeen = it.type == InputEventType.KEY; inputSeen }
        dispatcher.onDisplayCommand = { displaySeen = it.mode == DisplayMode.PHONE_CONTROL; displaySeen }

        val offer = FileOffer(ByteArray(16) { 1 }, 4, ByteArray(32), "note.txt", "text/plain")
        assertTrue(dispatcher.dispatch(Envelope(3, SessionFeatureDispatcher.KIND_FILE_OFFER, 0, 1, FileCodec.encodeOffer(offer))))
        assertTrue(fileSeen)

        val clipboard = ClipboardUpdate(ByteArray(16) { 2 }, "android", 10, "hello")
        assertTrue(dispatcher.dispatch(Envelope(4, SessionFeatureDispatcher.KIND_CLIPBOARD_UPDATE, 0, 2, ClipboardCodec.encode(clipboard))))
        assertTrue(clipboardSeen)

        val audio = AudioFrame(AudioCodec.OPUS, 1, 48_000, 1, 10, ByteArray(4) { 7 })
        assertTrue(dispatcher.dispatch(Envelope(5, SessionFeatureDispatcher.KIND_AUDIO_FRAME, 0, 3, AudioCodecWire.encode(audio))))
        assertTrue(audioSeen)

        val input = InputEvent(InputEventType.KEY, 1, 0, 0, 0, 0, 0x41, 0)
        assertTrue(dispatcher.dispatch(Envelope(1, SessionFeatureDispatcher.KIND_INPUT_EVENT, 0, 4, InputEventCodec.encode(input))))
        assertTrue(inputSeen)

        val display = DisplayCommand(DisplayMode.PHONE_CONTROL, true, 5)
        assertTrue(dispatcher.dispatch(Envelope(1, SessionFeatureDispatcher.KIND_DISPLAY_COMMAND, 0, 5, DisplayCommandCodec.encode(display))))
        assertTrue(displaySeen)
    }

    @Test
    fun rejectsUnsupportedAndMalformedFrames() {
        val dispatcher = SessionFeatureDispatcher()
        assertFalse(dispatcher.dispatch(Envelope(2, 1, 0, 1, byteArrayOf(1))))
        assertFalse(dispatcher.dispatch(Envelope(3, SessionFeatureDispatcher.KIND_FILE_OFFER, 0, 2, byteArrayOf(1, 2, 3))))
    }
}
