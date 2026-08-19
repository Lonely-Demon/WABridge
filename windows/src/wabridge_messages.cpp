#include "wabridge_messages.h"

#include "wabridge_envelope.h"

#include <algorithm>
#include <openssl/rand.h>
#include <stdexcept>

namespace wabridge::messages {
namespace {
constexpr std::size_t kDeviceIdMax = 64;
constexpr std::size_t kPayloadSize = 1 + 32 + 1 + 64 + 32 + 4;

void put_u32(std::vector<std::uint8_t>& output, const std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 24));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& input, const std::size_t offset) {
    return (static_cast<std::uint32_t>(input[offset]) << 24) |
           (static_cast<std::uint32_t>(input[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(input[offset + 2]) << 8) |
           static_cast<std::uint32_t>(input[offset + 3]);
}

void validate_hello(const SessionHello& hello) {
    if (hello.role != Role::Windows && hello.role != Role::Android) {
        throw protocol::ProtocolError("invalid hello role");
    }
    if (hello.device_id.empty() || hello.device_id.size() > kDeviceIdMax) {
        throw protocol::ProtocolError("invalid hello device id");
    }
    for (const unsigned char character : hello.device_id) {
        if (character < 0x20 || character > 0x7e) {
            throw protocol::ProtocolError("invalid hello device id characters");
        }
    }
    if (hello.max_frame == 0 || hello.max_frame > 4u * 1024u * 1024u) {
        throw protocol::ProtocolError("invalid hello max frame");
    }
}

} // namespace

std::array<std::uint8_t, 32> fresh_session_nonce() {
    std::array<std::uint8_t, 32> nonce{};
    if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) {
        throw protocol::ProtocolError("secure random nonce generation failed");
    }
    return nonce;
}

std::vector<std::uint8_t> encode_session_hello(const SessionHello& hello) {
    validate_hello(hello);
    std::vector<std::uint8_t> output;
    output.reserve(kPayloadSize);
    output.push_back(static_cast<std::uint8_t>(hello.role));
    output.insert(output.end(), hello.session_nonce.begin(), hello.session_nonce.end());
    output.push_back(static_cast<std::uint8_t>(hello.device_id.size()));
    output.insert(output.end(), hello.device_id.begin(), hello.device_id.end());
    output.insert(output.end(), hello.capabilities_hash.begin(), hello.capabilities_hash.end());
    put_u32(output, hello.max_frame);
    return output;
}

SessionHello decode_session_hello(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < 1 + 32 + 1 + 32 + 4) {
        throw protocol::ProtocolError("truncated session hello");
    }
    std::size_t offset = 0;
    SessionHello hello;
    hello.role = static_cast<Role>(payload[offset++]);
    std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(offset), hello.session_nonce.size(), hello.session_nonce.begin());
    offset += hello.session_nonce.size();
    const auto device_id_length = payload[offset++];
    if (device_id_length == 0 || device_id_length > kDeviceIdMax) {
        throw protocol::ProtocolError("invalid hello device id length");
    }
    const auto expected_size = std::size_t{1} + 32u + 1u +
                               static_cast<std::size_t>(device_id_length) + 32u + 4u;
    if (payload.size() != expected_size) {
        throw protocol::ProtocolError("session hello length mismatch");
    }
    hello.device_id.assign(reinterpret_cast<const char*>(payload.data() + offset), device_id_length);
    offset += device_id_length;
    std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(offset), hello.capabilities_hash.size(), hello.capabilities_hash.begin());
    offset += hello.capabilities_hash.size();
    hello.max_frame = read_u32(payload, offset);
    validate_hello(hello);
    return hello;
}

} // namespace wabridge::messages
