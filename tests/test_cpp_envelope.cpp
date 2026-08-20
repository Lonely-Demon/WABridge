#include "test_require.h"
#include "wabridge_envelope.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using wabridge::protocol::Envelope;
using wabridge::protocol::ProtocolError;
using wabridge::protocol::decode;
using wabridge::protocol::encode;

namespace {

void expect_error(const std::vector<std::uint8_t>& frame) {
    bool failed = false;
    try {
        (void)decode(frame);
    } catch (const ProtocolError&) {
        failed = true;
    }
    REQUIRE(failed);
}

void put_u32(std::vector<std::uint8_t>& frame, const std::size_t at, const std::uint32_t value) {
    frame[at] = static_cast<std::uint8_t>(value >> 24);
    frame[at + 1] = static_cast<std::uint8_t>(value >> 16);
    frame[at + 2] = static_cast<std::uint8_t>(value >> 8);
    frame[at + 3] = static_cast<std::uint8_t>(value);
}

} // namespace

int main() {
    const Envelope original{1, 1, 1, 42, {'h', 'e', 'l', 'l', 'o'}};
    const auto encoded = encode(original);
    const auto decoded = decode(encoded);
    REQUIRE(decoded.channel == original.channel);
    REQUIRE(decoded.kind == original.kind);
    REQUIRE(decoded.flags == original.flags);
    REQUIRE(decoded.request_id == original.request_id);
    REQUIRE(decoded.payload == original.payload);

    auto bad_magic = encoded;
    bad_magic[0] = 0;
    expect_error(bad_magic);

    auto truncated = encoded;
    truncated.resize(19);
    expect_error(truncated);

    auto wrong_length = encoded;
    put_u32(wrong_length, 16, 2);
    expect_error(wrong_length);

    auto oversized = encoded;
    put_u32(oversized, 16, 65 * 1024);
    oversized.resize(20 + 65 * 1024, 0);
    expect_error(oversized);

    auto zero_request = encoded;
    for (std::size_t i = 8; i < 16; ++i) {
        zero_request[i] = 0;
    }
    expect_error(zero_request);

    std::cout << "C++ envelope tests passed\n";
    return 0;
}
