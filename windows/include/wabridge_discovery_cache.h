#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wabridge::discovery {

struct ServiceEndpoint {
    std::string instance;
    std::string host;
    std::uint16_t port{};
    std::vector<std::string> addresses;
};

class RecordCache final {
public:
    using Clock = std::chrono::steady_clock;

    void apply_ptr(std::string_view instance, std::chrono::seconds ttl, Clock::time_point now);
    void apply_srv(std::string_view instance, std::string_view host, std::uint16_t port,
                   std::chrono::seconds ttl, Clock::time_point now);
    void apply_address(std::string_view host, std::string_view address,
                       std::chrono::seconds ttl, Clock::time_point now);

    std::vector<ServiceEndpoint> ready(Clock::time_point now) const;
    void expire(Clock::time_point now);
    void clear() noexcept;

    struct Entry {
        std::string instance;
        std::string host;
        std::uint16_t port{};
        std::vector<std::string> addresses;
        Clock::time_point ptr_expires{};
        Clock::time_point srv_expires{};
        Clock::time_point address_expires{};
    };

private:
    std::vector<Entry> entries_;
};

} // namespace wabridge::discovery
