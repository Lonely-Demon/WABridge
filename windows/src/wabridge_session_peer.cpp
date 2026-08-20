#include "wabridge_session_peer.h"

#include "wabridge_envelope.h"

namespace wabridge::session {

SessionPeer::SessionPeer(tls::Stream& stream, const messages::Role local_role,
                         const messages::Role peer_role) noexcept
    : stream_(stream), local_role_(local_role), peer_role_(peer_role) {}

bool SessionPeer::establish(const messages::SessionHello& local_hello) {
    if (established_ || local_hello.role != local_role_) return false;
    if (!stream_.handshake()) {
        stop();
        return false;
    }
    try {
        const auto local_payload = messages::encode_session_hello(local_hello);
        if (!stream_.write({1, 1, 1, 1, local_payload})) {
            stop();
            return false;
        }
        const auto received = stream_.read();
        if (!received.has_value() || received->channel != 1 || received->kind != 1 || received->request_id == 0) {
            stop();
            return false;
        }
        const auto peer = messages::decode_session_hello(received->payload);
        if (peer.role != peer_role_) {
            stop();
            return false;
        }
        peer_hello_ = peer;
        established_ = true;
        return true;
    } catch (const protocol::ProtocolError&) {
        stop();
        return false;
    }
}

bool SessionPeer::run(const std::function<bool(const protocol::Envelope&)>& handler) {
    if (!established_ || !handler) return false;
    while (established_) {
        const auto received = stream_.read();
        if (!received.has_value()) {
            established_ = false;
            return false;
        }
        if (!handler(*received)) {
            stop();
            return false;
        }
    }
    return true;
}

void SessionPeer::stop() noexcept {
    if (!established_ && !peer_hello_.has_value()) {
        stream_.close();
        return;
    }
    established_ = false;
    peer_hello_.reset();
    stream_.close();
}

} // namespace wabridge::session
