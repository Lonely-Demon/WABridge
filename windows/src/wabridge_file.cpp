#include "wabridge_file.h"

#include "wabridge_envelope.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace wabridge::file {
namespace {
constexpr std::size_t kMaxName = 255;
constexpr std::size_t kMaxMime = 128;
constexpr std::size_t kMaxChunk = 1 * 1024 * 1024;

void put_u32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void put_u64(std::vector<std::uint8_t>& out, const std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& input, const std::size_t at) {
    return (static_cast<std::uint32_t>(input[at]) << 24) |
           (static_cast<std::uint32_t>(input[at + 1]) << 16) |
           (static_cast<std::uint32_t>(input[at + 2]) << 8) |
           static_cast<std::uint32_t>(input[at + 3]);
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& input, const std::size_t at) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8) | input[at + i];
    return value;
}

void require_text(std::string_view value, std::size_t maximum, const char* field) {
    if (value.empty() || value.size() > maximum) throw protocol::ProtocolError(std::string("invalid ") + field);
    for (const unsigned char character : value) {
        if (character < 0x20 || character > 0x7e) throw protocol::ProtocolError(std::string("invalid ") + field);
    }
}

} // namespace

std::string safe_display_name(const std::string_view name) {
    require_text(name, kMaxName, "file name");
    std::string result;
    result.reserve(name.size());
    for (const unsigned char character : name) {
        if (character == '/' || character == '\\' || character == ':' || character == '\0') {
            result.push_back('_');
        } else {
            result.push_back(static_cast<char>(character));
        }
    }
    while (result == "." || result == "..") result.insert(result.begin(), '_');
    return result;
}

std::vector<std::uint8_t> encode_offer(const Offer& offer) {
    const auto name = safe_display_name(offer.display_name);
    require_text(offer.mime_type, kMaxMime, "mime type");
    if (offer.size == 0) throw protocol::ProtocolError("empty file offer");
    if (name.size() > 255 || offer.mime_type.size() > 255) throw protocol::ProtocolError("file metadata too long");

    std::vector<std::uint8_t> output;
    output.reserve(16 + 8 + 32 + 1 + name.size() + 1 + offer.mime_type.size());
    output.insert(output.end(), offer.transfer_id.begin(), offer.transfer_id.end());
    put_u64(output, offer.size);
    output.insert(output.end(), offer.sha256.begin(), offer.sha256.end());
    output.push_back(static_cast<std::uint8_t>(name.size()));
    output.insert(output.end(), name.begin(), name.end());
    output.push_back(static_cast<std::uint8_t>(offer.mime_type.size()));
    output.insert(output.end(), offer.mime_type.begin(), offer.mime_type.end());
    return output;
}

Offer decode_offer(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < 16 + 8 + 32 + 1 + 1) throw protocol::ProtocolError("truncated file offer");
    std::size_t offset = 0;
    Offer offer;
    std::copy_n(payload.begin(), offer.transfer_id.size(), offer.transfer_id.begin());
    offset += offer.transfer_id.size();
    offer.size = read_u64(payload, offset);
    offset += 8;
    std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(offset), offer.sha256.size(), offer.sha256.begin());
    offset += offer.sha256.size();
    const auto name_length = payload[offset++];
    if (name_length == 0 || name_length > kMaxName || offset + name_length + 1 > payload.size()) {
        throw protocol::ProtocolError("invalid file name length");
    }
    offer.display_name.assign(reinterpret_cast<const char*>(payload.data() + offset), name_length);
    offset += name_length;
    const auto mime_length = payload[offset++];
    if (mime_length == 0 || mime_length > kMaxMime || offset + mime_length != payload.size()) {
        throw protocol::ProtocolError("invalid mime type length");
    }
    offer.mime_type.assign(reinterpret_cast<const char*>(payload.data() + offset), mime_length);
    if (offer.size == 0) throw protocol::ProtocolError("empty file offer");
    offer.display_name = safe_display_name(offer.display_name);
    require_text(offer.mime_type, kMaxMime, "mime type");
    return offer;
}

std::vector<std::uint8_t> encode_chunk(const Chunk& chunk) {
    if (chunk.data.empty() || chunk.data.size() > kMaxChunk) throw protocol::ProtocolError("invalid file chunk size");
    std::vector<std::uint8_t> output;
    output.reserve(16 + 8 + 4 + chunk.data.size());
    output.insert(output.end(), chunk.transfer_id.begin(), chunk.transfer_id.end());
    put_u64(output, chunk.offset);
    put_u32(output, static_cast<std::uint32_t>(chunk.data.size()));
    output.insert(output.end(), chunk.data.begin(), chunk.data.end());
    return output;
}

Chunk decode_chunk(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < 16 + 8 + 4) throw protocol::ProtocolError("truncated file chunk");
    Chunk chunk;
    std::copy_n(payload.begin(), chunk.transfer_id.size(), chunk.transfer_id.begin());
    chunk.offset = read_u64(payload, 16);
    const auto length = read_u32(payload, 24);
    if (length == 0 || length > kMaxChunk || payload.size() != 28 + length) {
        throw protocol::ProtocolError("invalid file chunk length");
    }
    chunk.data.assign(payload.begin() + 28, payload.end());
    return chunk;
}

} // namespace wabridge::file
