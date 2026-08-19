#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace wabridge::file {

using TransferId = std::array<std::uint8_t, 16>;
using Digest = std::array<std::uint8_t, 32>;

struct Offer {
    TransferId transfer_id{};
    std::uint64_t size{};
    Digest sha256{};
    std::string display_name;
    std::string mime_type;
};

struct Chunk {
    TransferId transfer_id{};
    std::uint64_t offset{};
    std::vector<std::uint8_t> data;
};

std::vector<std::uint8_t> encode_offer(const Offer& offer);
Offer decode_offer(const std::vector<std::uint8_t>& payload);
std::vector<std::uint8_t> encode_chunk(const Chunk& chunk);
Chunk decode_chunk(const std::vector<std::uint8_t>& payload);
std::string safe_display_name(std::string_view name);

} // namespace wabridge::file
