#pragma once

#include "wabridge_messages.h"
#include "wabridge_tls_stream.h"

#include <functional>
#include <optional>

namespace wabridge::session {

class SessionPeer final {
public:
    SessionPeer(tls::Stream& stream, messages::Role local_role, messages::Role peer_role) noexcept;
    SessionPeer(const SessionPeer&) = delete;
    SessionPeer& operator=(const SessionPeer&) = delete;

    bool establish(const messages::SessionHello& local_hello);
    bool run(const std::function<bool(const protocol::Envelope&)>& handler);
    const std::optional<messages::SessionHello>& peer_hello() const noexcept { return peer_hello_; }
    bool established() const noexcept { return established_; }
    void stop() noexcept;

private:
    tls::Stream& stream_;
    messages::Role local_role_;
    messages::Role peer_role_;
    std::optional<messages::SessionHello> peer_hello_;
    bool established_{false};
};

} // namespace wabridge::session
