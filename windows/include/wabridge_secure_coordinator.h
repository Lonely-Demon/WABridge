#pragma once

#include "wabridge_coordinator.h"
#include "wabridge_messages.h"
#include "wabridge_session_peer.h"
#include "wabridge_tls.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace wabridge::coordinator {

class SecureCoordinator final {
public:
    SecureCoordinator(tls::Context context, messages::SessionHello local_hello);
    ~SecureCoordinator();
    SecureCoordinator(const SecureCoordinator&) = delete;
    SecureCoordinator& operator=(const SecureCoordinator&) = delete;

    bool start(std::uint16_t port);
    void stop() noexcept;
    bool running() const noexcept { return coordinator_.running(); }
    std::uint16_t port() const noexcept { return coordinator_.port(); }
    std::uint64_t established_sessions() const noexcept { return established_sessions_.load(); }

private:
    void handle_socket(net::Socket socket);

    tls::Context context_;
    messages::SessionHello local_hello_;
    Coordinator coordinator_;
    std::atomic<std::uint64_t> established_sessions_{0};
};

} // namespace wabridge::coordinator
