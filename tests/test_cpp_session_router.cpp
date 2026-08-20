#include "test_require.h"
#include "wabridge_session_router.h"

#include <iostream>

int main() {
    wabridge::session::Router router;
    bool handled = false;
    router.set_handler(3, [&](const wabridge::protocol::Envelope& envelope) {
        handled = envelope.kind == 7 && envelope.payload == std::vector<std::uint8_t>{1, 2, 3};
        return handled;
    });

    const wabridge::protocol::Envelope owned{3, 7, 0, 9, {1, 2, 3}};
    REQUIRE(router.dispatch(owned));
    REQUIRE(handled);
    REQUIRE(!router.dispatch({4, 1, 0, 1, {9}}));
    REQUIRE(!router.dispatch({0, 1, 0, 1, {9}}));

    bool rejected = false;
    try { router.set_handler(6, [](const auto&) { return true; }); }
    catch (const wabridge::protocol::ProtocolError&) { rejected = true; }
    REQUIRE(rejected);
    rejected = false;
    try { router.set_handler(3, {}); }
    catch (const wabridge::protocol::ProtocolError&) { rejected = true; }
    REQUIRE(rejected);

    std::cout << "Session-router tests passed\n";
    return 0;
}
