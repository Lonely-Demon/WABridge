#include "wabridge_session.h"

namespace wabridge::session {

const char* to_string(const State state) noexcept {
    switch (state) {
    case State::Idle: return "Idle";
    case State::Discovering: return "Discovering";
    case State::Connecting: return "Connecting";
    case State::TlsHandshaking: return "TlsHandshaking";
    case State::IdentityChecking: return "IdentityChecking";
    case State::PairingRequired: return "PairingRequired";
    case State::Established: return "Established";
    case State::Closing: return "Closing";
    case State::Failed: return "Failed";
    }
    return "Unknown";
}

bool StateMachine::apply(const Event event, std::string* reason) noexcept {
    auto reject = [&](const char* message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };

    if (event == Event::Stop) {
        stop();
        return true;
    }
    if (event == Event::Closed && state_ == State::Closing) {
        state_ = State::Idle;
        stopped_ = true;
        return true;
    }
    if (stopped_ && event != Event::BeginDiscovery) {
        return reject("session is stopped");
    }

    switch (state_) {
    case State::Idle:
        if (event == Event::BeginDiscovery) {
            state_ = State::Discovering;
            stopped_ = false;
            return true;
        }
        return reject("Idle expects BeginDiscovery");
    case State::Discovering:
        if (event == Event::CandidateFound) {
            state_ = State::Connecting;
            return true;
        }
        return reject("Discovering expects CandidateFound");
    case State::Connecting:
        if (event == Event::TlsStarted) {
            state_ = State::TlsHandshaking;
            return true;
        }
        return reject("Connecting expects TlsStarted");
    case State::TlsHandshaking:
        if (event == Event::TlsSucceeded) {
            state_ = State::IdentityChecking;
            return true;
        }
        if (event == Event::Failure) {
            state_ = State::Failed;
            return true;
        }
        return reject("TlsHandshaking expects TlsSucceeded or Failure");
    case State::IdentityChecking:
        if (event == Event::IdentityMatches) {
            state_ = State::Established;
            return true;
        }
        if (event == Event::PairingNeeded) {
            state_ = State::PairingRequired;
            return true;
        }
        if (event == Event::Failure) {
            state_ = State::Failed;
            return true;
        }
        return reject("IdentityChecking expects identity result");
    case State::PairingRequired:
        if (event == Event::PairingApproved) {
            state_ = State::Established;
            return true;
        }
        if (event == Event::Failure) {
            state_ = State::Failed;
            return true;
        }
        return reject("PairingRequired expects PairingApproved or Failure");
    case State::Established:
        if (event == Event::Failure) {
            state_ = State::Failed;
            return true;
        }
        return reject("Established expects Failure or Stop");
    case State::Closing:
        return reject("Closing expects Closed");
    case State::Failed:
        return reject("Failed expects Stop");
    }
    return reject("unknown state");
}

void StateMachine::stop() noexcept {
    if (stopped_) {
        return;
    }
    stopped_ = true;
    if (state_ != State::Idle) {
        state_ = State::Closing;
    }
}

} // namespace wabridge::session
