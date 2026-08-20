package com.wabridge.android.audio

import com.wabridge.android.protocol.ProtocolException
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class AudioFramesTest {
    @Test
    fun roundTripPcm16AndOpus() {
        val pcm = AudioFrame(AudioCodec.PCM16, 2, 48_000, 17, 1234, ByteArray(8) { 0x55 })
        val decoded = AudioCodecWire.decode(AudioCodecWire.encode(pcm))
        assertEquals(pcm.codec, decoded.codec)
        assertEquals(pcm.channels, decoded.channels)
        assertEquals(pcm.sampleRate, decoded.sampleRate)
        assertEquals(pcm.sequence, decoded.sequence)
        assertEquals(pcm.timestampMs, decoded.timestampMs)
        assertArrayEquals(pcm.data, decoded.data)

        val opus = AudioFrame(AudioCodec.OPUS, 1, 48_000, 18, 1235, byteArrayOf(1, 2, 3))
        assertArrayEquals(opus.data, AudioCodecWire.decode(AudioCodecWire.encode(opus)).data)
    }

    @Test
    fun rejectsInvalidPayloads() {
        assertThrows(IllegalArgumentException::class.java) {
            AudioFrame(AudioCodec.PCM16, 2, 48_000, 1, 1, ByteArray(3))
        }
        assertThrows(IllegalArgumentException::class.java) {
            AudioFrame(AudioCodec.PCM16, 2, 1, 1, 1, ByteArray(4))
        }
        assertThrows(ProtocolException::class.java) {
            AudioCodecWire.decode(ByteArray(AudioCodecWire.HEADER_SIZE - 1))
        }
    }
}
