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

void SecureCoordinator::stop() noexcept {
    coordinator_.stop();
}

void SecureCoordinator::handle_socket(net::Socket socket) {
    if (!socket.valid()) return;
    SSL* ssl = SSL_new(context_.native());
    if (ssl == nullptr) return;
    try {
        tls::Stream stream(ssl, socket.native(), tls::StreamMode::Server);
        session::SessionPeer peer(stream, messages::Role::Windows, messages::Role::Android);
        if (peer.establish(local_hello_)) established_sessions_.fetch_add(1);
    } catch (const tls::TlsError&) {
        // Stream owns and releases the SSL object on constructor failure.
    }
    socket.close();
}

} // namespace wabridge::coordinator
