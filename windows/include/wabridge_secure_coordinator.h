#pragma once

#include "wabridge_coordinator.h"
#include "wabridge_messages.h"
#include "wabridge_session_peer.h"
#include "wabridge_session_router.h"
#include "wabridge_feature_dispatch.h"
#include "wabridge_tls.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace wabridge::coordinator {

class SecureCoordinator final {
public:
    SecureCoordinator(tls::Context context, messages::SessionHello local_hello);
    ~SecureCoordinator();
    SecureCoordinator(const SecureCoordinator&) = delete;
    SecureCoordinator& operator=(const SecureCoordinator&) = delete;

    bool start(std::uint16_t port);
    void set_handler(std::uint8_t channel, session::Router::Handler handler);
    void set_feature_dispatcher(features::Dispatcher dispatcher);
    bool send(const protocol::Envelope& envelope);
    bool has_established_session() const noexcept;
    void stop() noexcept;
    bool running() const noexcept { return coordinator_.running(); }
    std::uint16_t port() const noexcept { return coordinator_.port(); }
    std::uint64_t established_sessions() const noexcept { return established_sessions_.load(); }

private:
    void handle_socket(net::Socket socket);

    tls::Context context_;
    messages::SessionHello local_hello_;
    Coordinator coordinator_;
    session::Router router_;
    features::Dispatcher feature_dispatcher_;
    std::atomic<std::uint64_t> established_sessions_{0};
    mutable std::mutex session_mutex_;
    std::shared_ptr<net::Socket> active_socket_;
    std::shared_ptr<tls::Stream> active_stream_;
    std::shared_ptr<session::SessionPeer> active_peer_;
};

} // namespace wabridge::coordinator
