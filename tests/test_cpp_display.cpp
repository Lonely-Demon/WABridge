#include "test_require.h"
#include "wabridge_display.h"
#include "wabridge_envelope.h"

#include <cassert>
#include <iostream>

using wabridge::display::Command;
using wabridge::display::Mode;
using wabridge::display::SuspendController;
using wabridge::display::decode;
using wabridge::display::encode;
using wabridge::protocol::ProtocolError;

int main() {
    const Command command{Mode::PhoneControl, false, 7};
    const auto decoded = decode(encode(command));
    REQUIRE(decoded.mode == command.mode);
    REQUIRE(decoded.suspended == command.suspended);
    REQUIRE(decoded.sequence == command.sequence);

    SuspendController controller;
    REQUIRE(!controller.suspended());
    REQUIRE(controller.toggle());
    REQUIRE(controller.suspended());
    REQUIRE(!controller.toggle());
    REQUIRE(!controller.suspended());
    controller.reset();
    controller.reset();
    REQUIRE(!controller.suspended());

    bool rejected = false;
    try {
        (void)decode({1, 0, 0, 0, 0, 0});
    } catch (const ProtocolError&) {
        rejected = true;
    }
    REQUIRE(rejected);

    rejected = false;
    try {
        (void)decode({99, 0, 0, 0, 0, 1});
    } catch (const ProtocolError&) {
        rejected = true;
    }
    REQUIRE(rejected);

    std::cout << "Display-control protocol tests passed\n";
    return 0;
}
