#pragma once

#include <string>

namespace wabridge::session {

enum class State {
    Idle,
    Discovering,
    Connecting,
    TlsHandshaking,
    IdentityChecking,
    PairingRequired,
    Established,
    Closing,
    Failed,
};

enum class Event {
    BeginDiscovery,
    CandidateFound,
    ConnectStarted,
    TlsStarted,
    TlsSucceeded,
    IdentityMatches,
    PairingNeeded,
    PairingApproved,
    Established,
    Failure,
    Stop,
    Closed,
};

class StateMachine final {
public:
    StateMachine() = default;

    State state() const noexcept { return state_; }
    bool stopped() const noexcept { return stopped_; }

    // Returns false without changing state when the event is invalid.
    bool apply(Event event, std::string* reason = nullptr) noexcept;

    // Safe in every state and safe to call repeatedly.
    void stop() noexcept;

private:
    State state_{State::Idle};
    bool stopped_{false};
};

const char* to_string(State state) noexcept;

} // namespace wabridge::session
