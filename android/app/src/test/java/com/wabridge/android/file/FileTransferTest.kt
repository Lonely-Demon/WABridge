package com.wabridge.android.file

import com.wabridge.android.protocol.ProtocolException
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class FileTransferTest {
    @Test
    fun offerRoundTrip() {
        val offer = FileOffer(ByteArray(16) { 1 }, 1234L, ByteArray(32) { 2 }, "../photo.jpg", "image/jpeg")
        val decoded = FileCodec.decodeOffer(FileCodec.encodeOffer(offer))
        assertEquals(offer.size, decoded.size)
        assertArrayEquals(offer.transferId, decoded.transferId)
        assertEquals(".._photo.jpg", decoded.displayName)
        assertEquals(offer.mimeType, decoded.mimeType)
    }

    @Test
    fun chunkRoundTrip() {
        val chunk = FileChunk(ByteArray(16) { 3 }, 4096L, ByteArray(512) { 4 })
        val decoded = FileCodec.decodeChunk(FileCodec.encodeChunk(chunk))
        assertEquals(chunk.offset, decoded.offset)
        assertArrayEquals(chunk.transferId, decoded.transferId)
        assertArrayEquals(chunk.data, decoded.data)
    }

    @Test
    fun rejectsMalformedChunk() {
        assertThrows(ProtocolException::class.java) { FileCodec.decodeChunk(ByteArray(10)) }
    }

    @Test
    fun sanitizesSeparators() {
        assertEquals("a_b_c_d", FileCodec.safeDisplayName("a:b\\c/d"))
    }
}
