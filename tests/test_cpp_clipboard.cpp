#include "wabridge_clipboard.h"
#include "wabridge_envelope.h"

#include <cassert>
#include <iostream>

using wabridge::clipboard::LoopGuard;
using wabridge::clipboard::Update;
using wabridge::clipboard::decode;
using wabridge::clipboard::encode;
using wabridge::protocol::ProtocolError;

int main() {
    Update update;
    update.loop_token.fill(0x44);
    update.origin_device_id = "desktop-test";
    update.timestamp_ms = 123456;
    update.text = "clipboard from WABridge";
    const auto decoded = decode(encode(update));
    assert(decoded.loop_token == update.loop_token);
    assert(decoded.origin_device_id == update.origin_device_id);
    assert(decoded.timestamp_ms == update.timestamp_ms);
    assert(decoded.text == update.text);

    LoopGuard guard;
    assert(guard.should_apply(update.loop_token));
    assert(!guard.should_apply(update.loop_token));
    std::array<std::uint8_t, 16> other{};
    other.fill(0x55);
    assert(guard.should_apply(other));
    guard.mark_local(update.loop_token);
    assert(!guard.should_apply(update.loop_token));

    update.timestamp_ms = 0;
    bool rejected = false;
    try {
        (void)encode(update);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    assert(rejected);

    update.timestamp_ms = 1;
    update.text.assign(1024 * 1024 + 1, 'x');
    rejected = false;
    try {
        (void)encode(update);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    assert(rejected);

    std::cout << "Clipboard protocol tests passed\n";
    return 0;
}
