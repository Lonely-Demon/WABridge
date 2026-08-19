#include "wabridge_endpoint.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace wabridge::discovery {
namespace {

std::string trim(std::string_view value) {
    std::size_t start = 0;
    std::size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start])) != 0) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) --end;
    return std::string(value.substr(start, end - start));
}

bool valid_host(std::string_view host) {
    if (host.empty() || host.size() > 253) return false;
    for (const unsigned char character : host) {
        if (std::iscntrl(character) != 0 || std::isspace(character) != 0) return false;
    }
    return true;
}

std::optional<std::uint16_t> parse_port(std::string_view text) {
    if (text.empty() || text.size() > 5) return std::nullopt;
    unsigned int port = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), port);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || port == 0 || port > 65535) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

std::optional<Endpoint> parse_endpoint(const std::string_view text,
                                       const std::uint16_t default_port) {
    const auto input = trim(text);
    if (input.empty()) return std::nullopt;

    std::string host;
    std::optional<std::uint16_t> port;
    if (input.front() == '[') {
        const auto closing = input.find(']');
        if (closing == std::string::npos || closing == 1) return std::nullopt;
        host = input.substr(1, closing - 1);
        if (closing + 1 < input.size()) {
            if (input[closing + 1] != ':') return std::nullopt;
            port = parse_port(std::string_view(input).substr(closing + 2));
            if (!port.has_value()) return std::nullopt;
        }
    } else {
        const auto first_colon = input.find(':');
        const auto last_colon = input.rfind(':');
        if (first_colon != std::string::npos && first_colon == last_colon) {
            host = input.substr(0, first_colon);
            port = parse_port(std::string_view(input).substr(first_colon + 1));
            if (!port.has_value()) return std::nullopt;
        } else {
            // A raw IPv6 literal must use brackets when a port is supplied.
            host = input;
        }
    }

    if (!valid_host(host)) return std::nullopt;
    if (!port.has_value()) {
        if (default_port == 0) return std::nullopt;
        port = default_port;
    }
    return Endpoint{std::move(host), *port};
}

} // namespace wabridge::discovery
