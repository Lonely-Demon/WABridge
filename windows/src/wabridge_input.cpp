#include "wabridge_input.h"

#include "wabridge_envelope.h"

#include <cstddef>
#include <limits>

namespace wabridge::input {
namespace {
constexpr std::size_t kSize = 1 + 1 + 2 + 4 + 4 + 4 + 4 + 1;
constexpr std::int32_t kCoordinateLimit = 32'767;
constexpr std::int32_t kWheelLimit = 1'000'000;

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

void put_i32(std::vector<std::uint8_t>& out, const std::int32_t value) {
    put_u32(out, static_cast<std::uint32_t>(value));
}

std::uint16_t get_u16(const std::vector<std::uint8_t>& payload, const std::size_t at) {
    return static_cast<std::uint16_t>((payload[at] << 8) | payload[at + 1]);
}

std::uint32_t get_u32(const std::vector<std::uint8_t>& payload, const std::size_t at) {
    return (static_cast<std::uint32_t>(payload[at]) << 24) |
           (static_cast<std::uint32_t>(payload[at + 1]) << 16) |
           (static_cast<std::uint32_t>(payload[at + 2]) << 8) |
           static_cast<std::uint32_t>(payload[at + 3]);
}

std::int32_t get_i32(const std::vector<std::uint8_t>& payload, const std::size_t at) {
    return static_cast<std::int32_t>(get_u32(payload, at));
}

void validate(const Event& event) {
    if (event.type != Type::MouseMove && event.type != Type::MouseButton &&
        event.type != Type::MouseWheel && event.type != Type::Key) {
        throw protocol::ProtocolError("invalid input event type");
    }
    if (event.x < -kCoordinateLimit || event.x > kCoordinateLimit ||
        event.y < -kCoordinateLimit || event.y > kCoordinateLimit) {
        throw protocol::ProtocolError("input coordinates out of range");
    }
    if (event.wheel < -kWheelLimit || event.wheel > kWheelLimit) {
        throw protocol::ProtocolError("input wheel delta out of range");
    }
    if (event.type == Type::MouseButton && (event.button == 0 || event.button > 8)) {
        throw protocol::ProtocolError("invalid mouse button");
    }
    if (event.type == Type::Key && (event.code == 0 || event.code > 0x10FFFFU)) {
        throw protocol::ProtocolError("invalid key code");
    }
    if (event.type != Type::Key && event.flags != 0) {
        throw protocol::ProtocolError("invalid non-key input flags");
    }
}
} // namespace

std::vector<std::uint8_t> encode_event(const Event& event) {
    validate(event);
    std::vector<std::uint8_t> output;
    output.reserve(kSize);
    output.push_back(static_cast<std::uint8_t>(event.type));
    output.push_back(event.flags);
    put_u16(output, event.modifiers);
    put_i32(output, event.x);
    put_i32(output, event.y);
    put_i32(output, event.wheel);
    put_u32(output, event.code);
    output.push_back(event.button);
    return output;
}

Event decode_event(const std::vector<std::uint8_t>& payload) {
    if (payload.size() != kSize) throw protocol::ProtocolError("invalid input event size");
    Event event;
    switch (payload[0]) {
    case static_cast<std::uint8_t>(Type::MouseMove): event.type = Type::MouseMove; break;
    case static_cast<std::uint8_t>(Type::MouseButton): event.type = Type::MouseButton; break;
    case static_cast<std::uint8_t>(Type::MouseWheel): event.type = Type::MouseWheel; break;
    case static_cast<std::uint8_t>(Type::Key): event.type = Type::Key; break;
    default: throw protocol::ProtocolError("invalid input event type");
    }
    event.flags = payload[1];
    event.modifiers = get_u16(payload, 2);
    event.x = get_i32(payload, 4);
    event.y = get_i32(payload, 8);
    event.wheel = get_i32(payload, 12);
    event.code = get_u32(payload, 16);
    event.button = payload[20];
    validate(event);
    return event;
}

} // namespace wabridge::input
