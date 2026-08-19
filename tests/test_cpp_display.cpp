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
    assert(decoded.mode == command.mode);
    assert(decoded.suspended == command.suspended);
    assert(decoded.sequence == command.sequence);

    SuspendController controller;
    assert(!controller.suspended());
    assert(controller.toggle());
    assert(controller.suspended());
    assert(!controller.toggle());
    assert(!controller.suspended());
    controller.reset();
    controller.reset();
    assert(!controller.suspended());

    bool rejected = false;
    try {
        (void)decode({1, 0, 0, 0, 0, 0});
    } catch (const ProtocolError&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        (void)decode({99, 0, 0, 0, 0, 1});
    } catch (const ProtocolError&) {
        rejected = true;
    }
    assert(rejected);

    std::cout << "Display-control protocol tests passed\n";
    return 0;
}
