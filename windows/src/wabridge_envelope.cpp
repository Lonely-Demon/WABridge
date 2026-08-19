#include "wabridge_envelope.h"

#include <algorithm>
#include <limits>

namespace wabridge::protocol {
namespace {

std::size_t channel_limit(const std::uint8_t channel) {
    switch (channel) {
    case 1: return 64u * 1024u;
    case 2: return 4u * 1024u * 1024u;
    case 3: return 1u * 1024u * 1024u;
    case 4: return 1u * 1024u * 1024u;
    case 5: return 256u * 1024u;
    default: throw ProtocolError("unknown channel");
    }
}

void put_u16(std::vector<std::uint8_t>& out, const std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void put_u32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void put_u64(std::vector<std::uint8_t>& out, const std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint16_t get_u16(const std::vector<std::uint8_t>& data, const std::size_t at) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[at]) << 8) | data[at + 1]);
}

std::uint32_t get_u32(const std::vector<std::uint8_t>& data, const std::size_t at) {
    return (static_cast<std::uint32_t>(data[at]) << 24) |
           (static_cast<std::uint32_t>(data[at + 1]) << 16) |
           (static_cast<std::uint32_t>(data[at + 2]) << 8) |
           static_cast<std::uint32_t>(data[at + 3]);
}

std::uint64_t get_u64(const std::vector<std::uint8_t>& data, const std::size_t at) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value = (value << 8) | data[at + i];
    }
    return value;
}

} // namespace

std::vector<std::uint8_t> encode(const Envelope& envelope) {
    const auto limit = channel_limit(envelope.channel);
    if (envelope.request_id == 0) {
        throw ProtocolError("zero request id");
    }
    if (envelope.payload.empty() || envelope.payload.size() > limit) {
        throw ProtocolError("payload exceeds channel limit");
    }
    if (envelope.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw ProtocolError("payload exceeds wire length");
    }

    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSize + envelope.payload.size());
    put_u16(out, kMagic);
    out.push_back(kVersion);
    out.push_back(envelope.channel);
    put_u16(out, envelope.kind);
    put_u16(out, envelope.flags);
    put_u64(out, envelope.request_id);
    put_u32(out, static_cast<std::uint32_t>(envelope.payload.size()));
    out.insert(out.end(), envelope.payload.begin(), envelope.payload.end());
    return out;
}

Envelope decode(const std::vector<std::uint8_t>& frame) {
    if (frame.size() < kHeaderSize) {
        throw ProtocolError("truncated header");
    }
    if (get_u16(frame, 0) != kMagic) {
        throw ProtocolError("bad magic");
    }
    if (frame[2] != kVersion) {
        throw ProtocolError("unsupported version");
    }
    const auto channel = frame[3];
    const auto limit = channel_limit(channel);
    const auto length = get_u32(frame, 16);
    if (get_u64(frame, 8) == 0) {
        throw ProtocolError("zero request id");
    }
    if (length == 0 || length > limit) {
        throw ProtocolError("invalid payload length");
    }
    if (frame.size() != kHeaderSize + static_cast<std::size_t>(length)) {
        throw ProtocolError("frame length mismatch");
    }

    Envelope result;
    result.channel = channel;
    result.kind = get_u16(frame, 4);
    result.flags = get_u16(frame, 6);
    result.request_id = get_u64(frame, 8);
    result.payload.assign(frame.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), frame.end());
    return result;
}

} // namespace wabridge::protocol
