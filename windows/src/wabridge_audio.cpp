#include "wabridge_audio.h"

#include "wabridge_envelope.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace wabridge::audio {
namespace {
constexpr std::size_t kHeaderSize = 1 + 1 + 4 + 8 + 8 + 4;
constexpr std::size_t kMaxData = 256 * 1024;
constexpr std::uint32_t kMinRate = 8'000;
constexpr std::uint32_t kMaxRate = 192'000;

void put_u32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void put_u64(std::vector<std::uint8_t>& out, const std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t get_u32(const std::vector<std::uint8_t>& payload, const std::size_t at) {
    return (static_cast<std::uint32_t>(payload[at]) << 24) |
           (static_cast<std::uint32_t>(payload[at + 1]) << 16) |
           (static_cast<std::uint32_t>(payload[at + 2]) << 8) |
           static_cast<std::uint32_t>(payload[at + 3]);
}

std::uint64_t get_u64(const std::vector<std::uint8_t>& payload, const std::size_t at) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index) value = (value << 8) | payload[at + index];
    return value;
}

void validate(const Frame& frame) {
    if (frame.codec != Codec::Pcm16 && frame.codec != Codec::Opus) {
        throw protocol::ProtocolError("invalid audio codec");
    }
    if (frame.channels == 0 || frame.channels > 8) throw protocol::ProtocolError("invalid audio channels");
    if (frame.sample_rate < kMinRate || frame.sample_rate > kMaxRate) {
        throw protocol::ProtocolError("invalid audio sample rate");
    }
    if (frame.timestamp_ms == 0) throw protocol::ProtocolError("invalid audio timestamp");
    if (frame.data.empty() || frame.data.size() > kMaxData) throw protocol::ProtocolError("invalid audio payload");
    if (frame.codec == Codec::Pcm16 && frame.data.size() % (2U * frame.channels) != 0) {
        throw protocol::ProtocolError("unaligned PCM16 payload");
    }
}
} // namespace

std::vector<std::uint8_t> encode_frame(const Frame& frame) {
    validate(frame);
    std::vector<std::uint8_t> output;
    output.reserve(kHeaderSize + frame.data.size());
    output.push_back(static_cast<std::uint8_t>(frame.codec));
    output.push_back(frame.channels);
    put_u32(output, frame.sample_rate);
    put_u64(output, frame.sequence);
    put_u64(output, frame.timestamp_ms);
    put_u32(output, static_cast<std::uint32_t>(frame.data.size()));
    output.insert(output.end(), frame.data.begin(), frame.data.end());
    return output;
}

Frame decode_frame(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < kHeaderSize) throw protocol::ProtocolError("truncated audio frame");
    const auto raw_codec = payload[0];
    Frame frame;
    if (raw_codec == static_cast<std::uint8_t>(Codec::Pcm16)) frame.codec = Codec::Pcm16;
    else if (raw_codec == static_cast<std::uint8_t>(Codec::Opus)) frame.codec = Codec::Opus;
    else throw protocol::ProtocolError("invalid audio codec");
    frame.channels = payload[1];
    frame.sample_rate = get_u32(payload, 2);
    frame.sequence = get_u64(payload, 6);
    frame.timestamp_ms = get_u64(payload, 14);
    const auto length = get_u32(payload, 22);
    if (length == 0 || length > kMaxData || payload.size() != kHeaderSize + length) {
        throw protocol::ProtocolError("invalid audio payload length");
    }
    frame.data.assign(payload.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), payload.end());
    validate(frame);
    return frame;
}

} // namespace wabridge::audio
