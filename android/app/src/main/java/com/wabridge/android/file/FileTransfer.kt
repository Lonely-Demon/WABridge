package com.wabridge.android.file

import com.wabridge.android.protocol.ProtocolException
import java.nio.ByteBuffer
import java.nio.ByteOrder

data class FileOffer(
    val transferId: ByteArray,
    val size: Long,
    val sha256: ByteArray,
    val displayName: String,
    val mimeType: String,
) {
    init {
        require(transferId.size == 16)
        require(sha256.size == 32)
        require(size > 0)
    }
}

data class FileChunk(
    val transferId: ByteArray,
    val offset: Long,
    val data: ByteArray,
) {
    init {
        require(transferId.size == 16)
        require(data.isNotEmpty() && data.size <= MAX_CHUNK)
    }
}

object FileCodec {
    fun encodeOffer(offer: FileOffer): ByteArray {
        val name = safeDisplayName(offer.displayName)
        requireAscii(name, MAX_NAME, "file name")
        requireAscii(offer.mimeType, MAX_MIME, "mime type")
        val nameBytes = name.toByteArray(Charsets.US_ASCII)
        val mimeBytes = offer.mimeType.toByteArray(Charsets.US_ASCII)
        return ByteBuffer.allocate(16 + 8 + 32 + 1 + nameBytes.size + 1 + mimeBytes.size)
            .order(ByteOrder.BIG_ENDIAN)
            .put(offer.transferId)
            .putLong(offer.size)
            .put(offer.sha256)
            .put(nameBytes.size.toByte())
            .put(nameBytes)
            .put(mimeBytes.size.toByte())
            .put(mimeBytes)
            .array()
    }

    fun decodeOffer(payload: ByteArray): FileOffer {
        if (payload.size < 16 + 8 + 32 + 2) throw ProtocolException("truncated file offer")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN)
        val transferId = ByteArray(16).also(buffer::get)
        val size = buffer.long
        val digest = ByteArray(32).also(buffer::get)
        val nameLength = buffer.get().toInt() and 0xff
        if (nameLength == 0 || nameLength > MAX_NAME || buffer.remaining() < nameLength + 1) {
            throw ProtocolException("invalid file name length")
        }
        val name = ByteArray(nameLength).also(buffer::get).toString(Charsets.US_ASCII)
        val mimeLength = buffer.get().toInt() and 0xff
        if (mimeLength == 0 || mimeLength > MAX_MIME || buffer.remaining() != mimeLength) {
            throw ProtocolException("invalid mime length")
        }
        val mime = ByteArray(mimeLength).also(buffer::get).toString(Charsets.US_ASCII)
        return try {
            FileOffer(transferId, size, digest, safeDisplayName(name), mime)
        } catch (error: IllegalArgumentException) {
            throw ProtocolException(error.message ?: "invalid file offer")
        }
    }

    fun encodeChunk(chunk: FileChunk): ByteArray = ByteBuffer.allocate(16 + 8 + 4 + chunk.data.size)
        .order(ByteOrder.BIG_ENDIAN)
        .put(chunk.transferId)
        .putLong(chunk.offset)
        .putInt(chunk.data.size)
        .put(chunk.data)
        .array()

    fun decodeChunk(payload: ByteArray): FileChunk {
        if (payload.size < 28) throw ProtocolException("truncated file chunk")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN)
        val transferId = ByteArray(16).also(buffer::get)
        val offset = buffer.long
        val length = buffer.int
        if (length <= 0 || length > MAX_CHUNK || buffer.remaining() != length) {
            throw ProtocolException("invalid file chunk length")
        }
        return FileChunk(transferId, offset, ByteArray(length).also(buffer::get))
    }

    fun safeDisplayName(name: String): String {
        requireAscii(name, MAX_NAME, "file name")
        var result = name.map { if (it == '/' || it == '\\' || it == ':') '_' else it }.joinToString("")
        while (result == "." || result == "..") result = "_$result"
        return result
    }

    private fun requireAscii(value: String, maximum: Int, field: String) {
        if (value.isEmpty() || value.length > maximum || value.any { it.code !in 0x20..0x7e }) {
            throw ProtocolException("invalid $field")
        }
    }

    const val MAX_NAME = 255
    const val MAX_MIME = 128
    const val MAX_CHUNK = 1024 * 1024
}
