#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wabridge::discovery {

struct Endpoint {
    std::string host;
    std::uint16_t port{};
};

std::optional<Endpoint> parse_endpoint(std::string_view text,
                                       std::uint16_t default_port = 0);

} // namespace wabridge::discovery
