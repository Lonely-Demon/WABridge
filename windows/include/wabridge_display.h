#pragma once

#include <cstdint>
#include <vector>

namespace wabridge::display {

enum class Mode : std::uint8_t { None = 0, SecondDisplay = 1, PhoneControl = 2 };

struct Command {
    Mode mode{Mode::None};
    bool suspended{false};
    std::uint32_t sequence{};
};

std::vector<std::uint8_t> encode(const Command& command);
Command decode(const std::vector<std::uint8_t>& payload);

class SuspendController final {
public:
    bool toggle();
    bool suspended() const noexcept { return suspended_; }
    void reset() noexcept { suspended_ = false; }

private:
    bool suspended_{false};
};

} // namespace wabridge::display
