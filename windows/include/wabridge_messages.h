#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace wabridge::messages {

constexpr std::uint16_t kSessionHello = 0x0001;
constexpr std::uint16_t kCapabilities = 0x0002;
constexpr std::uint16_t kPairingStatus = 0x0003;
constexpr std::uint16_t kSasConfirmation = 0x0004;
constexpr std::uint16_t kHeartbeat = 0x0005;
constexpr std::uint16_t kHeartbeatAck = 0x0006;
constexpr std::uint16_t kSessionClose = 0x0007;
constexpr std::uint16_t kError = 0x0008;

enum class Role : std::uint8_t { Windows = 1, Android = 2 };

struct SessionHello {
    Role role{Role::Windows};
    std::array<std::uint8_t, 32> session_nonce{};
    std::string device_id;
    std::array<std::uint8_t, 32> capabilities_hash{};
    std::uint32_t max_frame{};
};

std::array<std::uint8_t, 32> fresh_session_nonce();
std::vector<std::uint8_t> encode_session_hello(const SessionHello& hello);
SessionHello decode_session_hello(const std::vector<std::uint8_t>& payload);

} // namespace wabridge::messages
