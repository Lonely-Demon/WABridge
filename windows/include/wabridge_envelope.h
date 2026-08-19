#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace wabridge::protocol {

constexpr std::uint16_t kMagic = 0x5742;
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kHeaderSize = 20;

class ProtocolError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Envelope {
    std::uint8_t channel{};
    std::uint16_t kind{};
    std::uint16_t flags{};
    std::uint64_t request_id{};
    std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> encode(const Envelope& envelope);
Envelope decode(const std::vector<std::uint8_t>& frame);

} // namespace wabridge::protocol
