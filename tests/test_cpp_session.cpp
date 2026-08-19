#include "wabridge_session.h"

#include <cassert>
#include <iostream>

using wabridge::session::Event;
using wabridge::session::State;
using wabridge::session::StateMachine;

int main() {
    StateMachine machine;
    assert(machine.state() == State::Idle);
    assert(!machine.apply(Event::CandidateFound));

    assert(machine.apply(Event::BeginDiscovery));
    assert(machine.apply(Event::CandidateFound));
    assert(machine.apply(Event::TlsStarted));
    assert(machine.apply(Event::TlsSucceeded));
    assert(machine.apply(Event::PairingNeeded));
    assert(machine.state() == State::PairingRequired);
    assert(machine.apply(Event::PairingApproved));
    assert(machine.state() == State::Established);

    machine.stop();
    assert(machine.state() == State::Closing);
    machine.stop();
    assert(machine.state() == State::Closing);
    assert(machine.apply(Event::Closed));
    assert(machine.state() == State::Idle);
    assert(machine.stopped());

    assert(machine.apply(Event::BeginDiscovery));
    assert(machine.state() == State::Discovering);

    std::cout << "C++ session tests passed\n";
    return 0;
}
