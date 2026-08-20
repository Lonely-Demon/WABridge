#include "test_require.h"
#include "wabridge_session.h"

#include <cassert>
#include <iostream>

using wabridge::session::Event;
using wabridge::session::State;
using wabridge::session::StateMachine;

int main() {
    StateMachine machine;
    REQUIRE(machine.state() == State::Idle);
    REQUIRE(!machine.apply(Event::CandidateFound));

    REQUIRE(machine.apply(Event::BeginDiscovery));
    REQUIRE(machine.apply(Event::CandidateFound));
    REQUIRE(machine.apply(Event::TlsStarted));
    REQUIRE(machine.apply(Event::TlsSucceeded));
    REQUIRE(machine.apply(Event::PairingNeeded));
    REQUIRE(machine.state() == State::PairingRequired);
    REQUIRE(machine.apply(Event::PairingApproved));
    REQUIRE(machine.state() == State::Established);

    machine.stop();
    REQUIRE(machine.state() == State::Closing);
    machine.stop();
    REQUIRE(machine.state() == State::Closing);
    REQUIRE(machine.apply(Event::Closed));
    REQUIRE(machine.state() == State::Idle);
    REQUIRE(machine.stopped());

    REQUIRE(machine.apply(Event::BeginDiscovery));
    REQUIRE(machine.state() == State::Discovering);

    std::cout << "C++ session tests passed\n";
    return 0;
}
