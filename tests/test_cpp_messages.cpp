#include "test_require.h"
#include "wabridge_messages.h"
#include "wabridge_envelope.h"

#include <cassert>
#include <iostream>

using wabridge::messages::Role;
using wabridge::messages::SessionHello;
using wabridge::messages::decode_session_hello;
using wabridge::messages::encode_session_hello;
using wabridge::messages::fresh_session_nonce;
using wabridge::protocol::ProtocolError;

int main() {
    SessionHello hello;
    hello.role = Role::Windows;
    hello.session_nonce = fresh_session_nonce();
    hello.device_id = "desktop-test";
    hello.capabilities_hash.fill(0xAB);
    hello.max_frame = 1024 * 1024;

    const auto encoded = encode_session_hello(hello);
    const auto decoded = decode_session_hello(encoded);
    REQUIRE(decoded.role == hello.role);
    REQUIRE(decoded.session_nonce == hello.session_nonce);
    REQUIRE(decoded.device_id == hello.device_id);
    REQUIRE(decoded.capabilities_hash == hello.capabilities_hash);
    REQUIRE(decoded.max_frame == hello.max_frame);

    const auto second_nonce = fresh_session_nonce();
    REQUIRE(second_nonce != hello.session_nonce);

    auto malformed = encoded;
    malformed.pop_back();
    bool rejected = false;
    try {
        (void)decode_session_hello(malformed);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    REQUIRE(rejected);

    hello.device_id.assign(65, 'x');
    rejected = false;
    try {
        (void)encode_session_hello(hello);
    } catch (const ProtocolError&) {
        rejected = true;
    }
    REQUIRE(rejected);

    std::cout << "SESSION_HELLO tests passed\n";
    return 0;
}
