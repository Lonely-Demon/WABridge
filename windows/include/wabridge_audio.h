#pragma once

#include <cstdint>
#include <vector>

namespace wabridge::audio {

enum class Codec : std::uint8_t { Pcm16 = 1, Opus = 2 };

struct Frame {
    Codec codec{Codec::Pcm16};
    std::uint8_t channels{};
    std::uint32_t sample_rate{};
    std::uint64_t sequence{};
    std::uint64_t timestamp_ms{};
    std::vector<std::uint8_t> data;
};

std::vector<std::uint8_t> encode_frame(const Frame& frame);
Frame decode_frame(const std::vector<std::uint8_t>& payload);

} // namespace wabridge::audio
