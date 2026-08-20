#pragma once

#include "wabridge_envelope.h"

#include <array>
#include <cstdint>
#include <functional>

namespace wabridge::session {

class Router final {
public:
    using Handler = std::function<bool(const protocol::Envelope&)>;

    void set_handler(std::uint8_t channel, Handler handler);
    bool dispatch(const protocol::Envelope& envelope) const;

private:
    std::array<Handler, 6> handlers_{};
};

} // namespace wabridge::session
