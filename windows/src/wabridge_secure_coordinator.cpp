#include "wabridge_secure_coordinator.h"

#include <openssl/ssl.h>

#include <utility>

namespace wabridge::coordinator {

SecureCoordinator::SecureCoordinator(tls::Context context, messages::SessionHello local_hello)
    : context_(std::move(context)), local_hello_(std::move(local_hello)),
      coordinator_([this](net::Socket socket) { handle_socket(std::move(socket)); }) {}

SecureCoordinator::~SecureCoordinator() {
    stop();
}

bool SecureCoordinator::start(const std::uint16_t port) {
    if (local_hello_.role != messages::Role::Windows) return false;
    return coordinator_.start(port);
}

void SecureCoordinator::set_handler(const std::uint8_t channel, session::Router::Handler handler) {
    router_.set_handler(channel, std::move(handler));
}

void SecureCoordinator::set_feature_dispatcher(features::Dispatcher dispatcher) {
    feature_dispatcher_ = std::move(dispatcher);
    router_.set_handler(1, [this](const protocol::Envelope& envelope) {
        return feature_dispatcher_.dispatch(envelope);
    });
    router_.set_handler(3, [this](const protocol::Envelope& envelope) {
        return feature_dispatcher_.dispatch(envelope);
    });
    router_.set_handler(4, [this](const protocol::Envelope& envelope) {
        return feature_dispatcher_.dispatch(envelope);
    });
    router_.set_handler(5, [this](const protocol::Envelope& envelope) {
        return feature_dispatcher_.dispatch(envelope);
    });
}

bool SecureCoordinator::send(const protocol::Envelope& envelope) {
    std::shared_ptr<session::SessionPeer> peer;
    {
        std::lock_guard lock(session_mutex_);
        peer = active_peer_;
    }
    return peer && peer->send(envelope);
}

bool SecureCoordinator::has_established_session() const noexcept {
    std::lock_guard lock(session_mutex_);
    return static_cast<bool>(active_peer_);
}

void SecureCoordinator::stop() noexcept {
    std::shared_ptr<session::SessionPeer> peer;
    {
        std::lock_guard lock(session_mutex_);
        peer = active_peer_;
    }
    if (peer) peer->stop();
    coordinator_.stop();
    {
        std::lock_guard lock(session_mutex_);
        active_peer_.reset();
        active_stream_.reset();
        active_socket_.reset();
    }
}

void SecureCoordinator::handle_socket(net::Socket socket) {
    if (!socket.valid()) return;
    auto owned_socket = std::make_shared<net::Socket>(std::move(socket));
    SSL* ssl = SSL_new(context_.native());
    if (ssl == nullptr) return;
    try {
        auto stream = std::make_shared<tls::Stream>(ssl, owned_socket->native(), tls::StreamMode::Server);
        auto peer = std::make_shared<session::SessionPeer>(*stream, messages::Role::Windows, messages::Role::Android);
        if (peer->establish(local_hello_)) {
            {
                std::lock_guard lock(session_mutex_);
                if (active_peer_) {
                    peer->stop();
                    return;
                }
                active_socket_ = owned_socket;
                active_stream_ = stream;
                active_peer_ = peer;
            }
            established_sessions_.fetch_add(1);
            (void)peer->run([this, peer](const protocol::Envelope& envelope) {
                if (envelope.channel == 1 && envelope.kind == messages::kHeartbeat) {
                    return peer->send({1, messages::kHeartbeatAck, 0, envelope.request_id, {1}});
                }
                if (envelope.channel == 1 && envelope.kind == messages::kSessionClose) {
                    return false;
                }
                return router_.dispatch(envelope);
            });
            peer->stop();
            {
                std::lock_guard lock(session_mutex_);
                if (active_peer_ == peer) {
                    active_peer_.reset();
                    active_stream_.reset();
                    active_socket_.reset();
                }
            }
        }
    } catch (const tls::TlsError&) {
        // Stream owns and releases the SSL object on constructor failure.
    }
}

} // namespace wabridge::coordinator
