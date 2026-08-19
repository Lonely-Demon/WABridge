#pragma once

#include "wabridge_socket.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace wabridge::coordinator {

class Coordinator final {
public:
    using AcceptHandler = std::function<void(net::Socket)>;

    explicit Coordinator(AcceptHandler handler = {});
    ~Coordinator();
    Coordinator(const Coordinator&) = delete;
    Coordinator& operator=(const Coordinator&) = delete;

    bool start(std::uint16_t port);
    void stop() noexcept;
    bool running() const noexcept { return running_.load(); }
    std::uint16_t port() const noexcept;

private:
    void accept_loop();

    AcceptHandler handler_;
    mutable std::mutex mutex_;
    std::optional<net::Socket> listener_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace wabridge::coordinator
