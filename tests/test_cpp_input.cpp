#include "test_require.h"
#include "wabridge_input.h"
#include "wabridge_envelope.h"

#include <iostream>

int main() {
    wabridge::input::Event move;
    move.type = wabridge::input::Type::MouseMove;
    move.modifiers = 3;
    move.x = 120;
    move.y = -45;
    auto decoded = wabridge::input::decode_event(wabridge::input::encode_event(move));
    REQUIRE(decoded.type == move.type);
    REQUIRE(decoded.modifiers == move.modifiers);
    REQUIRE(decoded.x == move.x);
    REQUIRE(decoded.y == move.y);

    wabridge::input::Event key;
    key.type = wabridge::input::Type::Key;
    key.flags = 1;
    key.modifiers = 4;
    key.code = 0x41;
    decoded = wabridge::input::decode_event(wabridge::input::encode_event(key));
    REQUIRE(decoded.type == key.type);
    REQUIRE(decoded.flags == key.flags);
    REQUIRE(decoded.code == key.code);

    wabridge::input::Event button;
    button.type = wabridge::input::Type::MouseButton;
    button.button = 1;
    decoded = wabridge::input::decode_event(wabridge::input::encode_event(button));
    REQUIRE(decoded.button == button.button);

    bool rejected = false;
    move.x = 40'000;
    try { (void)wabridge::input::encode_event(move); } catch (const wabridge::protocol::ProtocolError&) { rejected = true; }
    REQUIRE(rejected);

    button.button = 0;
    rejected = false;
    try { (void)wabridge::input::encode_event(button); } catch (const wabridge::protocol::ProtocolError&) { rejected = true; }
    REQUIRE(rejected);

    key.code = 0;
    rejected = false;
    try { (void)wabridge::input::encode_event(key); } catch (const wabridge::protocol::ProtocolError&) { rejected = true; }
    REQUIRE(rejected);

    std::cout << "Input-event codec tests passed\n";
    return 0;
}
