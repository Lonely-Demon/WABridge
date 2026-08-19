#include "wabridge_display.h"

#include "wabridge_envelope.h"

namespace wabridge::display {

std::vector<std::uint8_t> encode(const Command& command) {
    if (command.mode != Mode::None && command.mode != Mode::SecondDisplay && command.mode != Mode::PhoneControl) {
        throw protocol::ProtocolError("invalid display mode");
    }
    return {
        static_cast<std::uint8_t>(command.mode),
        static_cast<std::uint8_t>(command.suspended ? 1 : 0),
        static_cast<std::uint8_t>(command.sequence >> 24),
        static_cast<std::uint8_t>(command.sequence >> 16),
        static_cast<std::uint8_t>(command.sequence >> 8),
        static_cast<std::uint8_t>(command.sequence),
    };
}

Command decode(const std::vector<std::uint8_t>& payload) {
    if (payload.size() != 6) throw protocol::ProtocolError("invalid display command length");
    const auto mode = static_cast<Mode>(payload[0]);
    if (mode != Mode::None && mode != Mode::SecondDisplay && mode != Mode::PhoneControl) {
        throw protocol::ProtocolError("invalid display mode");
    }
    if (payload[1] > 1) throw protocol::ProtocolError("invalid display suspend flag");
    const auto sequence = (static_cast<std::uint32_t>(payload[2]) << 24) |
                          (static_cast<std::uint32_t>(payload[3]) << 16) |
                          (static_cast<std::uint32_t>(payload[4]) << 8) |
                          static_cast<std::uint32_t>(payload[5]);
    if (sequence == 0) throw protocol::ProtocolError("invalid display sequence");
    return {mode, payload[1] == 1, sequence};
}

bool SuspendController::toggle() {
    suspended_ = !suspended_;
    return suspended_;
}

} // namespace wabridge::display
