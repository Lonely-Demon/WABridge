#include "test_require.h"
#include "wabridge_audio.h"
#include "wabridge_envelope.h"

#include <iostream>

int main() {
    wabridge::audio::Frame frame;
    frame.codec = wabridge::audio::Codec::Pcm16;
    frame.channels = 2;
    frame.sample_rate = 48'000;
    frame.sequence = 17;
    frame.timestamp_ms = 1234;
    frame.data.assign(8, 0x55);
    const auto decoded = wabridge::audio::decode_frame(wabridge::audio::encode_frame(frame));
    REQUIRE(decoded.codec == frame.codec);
    REQUIRE(decoded.channels == frame.channels);
    REQUIRE(decoded.sample_rate == frame.sample_rate);
    REQUIRE(decoded.sequence == frame.sequence);
    REQUIRE(decoded.timestamp_ms == frame.timestamp_ms);
    REQUIRE(decoded.data == frame.data);

    frame.codec = wabridge::audio::Codec::Opus;
    frame.data.assign(3, 0xAA);
    REQUIRE(wabridge::audio::decode_frame(wabridge::audio::encode_frame(frame)).data == frame.data);

    bool rejected = false;
    frame.codec = wabridge::audio::Codec::Pcm16;
    frame.data.assign(3, 0);
    try { (void)wabridge::audio::encode_frame(frame); } catch (const wabridge::protocol::ProtocolError&) { rejected = true; }
    REQUIRE(rejected);

    frame.data.assign(8, 0);
    frame.sample_rate = 1;
    rejected = false;
    try { (void)wabridge::audio::encode_frame(frame); } catch (const wabridge::protocol::ProtocolError&) { rejected = true; }
    REQUIRE(rejected);

    std::cout << "Audio-frame codec tests passed\n";
    return 0;
}
