#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace wabridge::clipboard {

struct Update {
    std::array<std::uint8_t, 16> loop_token{};
    std::string origin_device_id;
    std::uint64_t timestamp_ms{};
    std::string text;
};

std::vector<std::uint8_t> encode(const Update& update);
Update decode(const std::vector<std::uint8_t>& payload);

class LoopGuard final {
public:
    bool should_apply(const std::array<std::uint8_t, 16>& token);
    void mark_local(const std::array<std::uint8_t, 16>& token);

private:
    std::array<std::uint8_t, 16> last_token_{};
    bool has_token_{false};
};

} // namespace wabridge::clipboard
