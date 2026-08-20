#pragma once

#include <cstdint>
#include <vector>

namespace wabridge::input {

enum class Type : std::uint8_t { MouseMove = 1, MouseButton = 2, MouseWheel = 3, Key = 4 };

struct Event {
    Type type{Type::MouseMove};
    std::uint8_t flags{};
    std::uint16_t modifiers{};
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t wheel{};
    std::uint32_t code{};
    std::uint8_t button{};
};

std::vector<std::uint8_t> encode_event(const Event& event);
Event decode_event(const std::vector<std::uint8_t>& payload);

} // namespace wabridge::input
