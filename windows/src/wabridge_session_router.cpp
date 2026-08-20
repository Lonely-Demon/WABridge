#include "wabridge_session_router.h"

namespace wabridge::session {

void Router::set_handler(const std::uint8_t channel, Handler handler) {
    if (channel == 0 || channel >= handlers_.size() || !handler) {
        throw protocol::ProtocolError("invalid session-router handler");
    }
    handlers_[channel] = std::move(handler);
}

bool Router::dispatch(const protocol::Envelope& envelope) const {
    if (envelope.channel == 0 || envelope.channel >= handlers_.size()) return false;
    const auto& handler = handlers_[envelope.channel];
    return handler && handler(envelope);
}

} // namespace wabridge::session
