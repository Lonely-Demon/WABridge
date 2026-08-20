#include "test_require.h"
#include "wabridge_wasapi_audio.h"

#include <iostream>

int main() {
    wabridge::platform_audio::WasapiRenderer renderer;
    REQUIRE(!renderer.running());
    REQUIRE(!renderer.start(0, 1));
    REQUIRE(!renderer.start(48'000, 0));
    wabridge::audio::Frame frame;
    frame.codec = wabridge::audio::Codec::Pcm16;
    frame.channels = 1;
    frame.sample_rate = 48'000;
    frame.timestamp_ms = 1;
    frame.data = {0, 0};
    REQUIRE(!renderer.render(frame));
    renderer.stop();
    renderer.stop();
    REQUIRE(!renderer.running());
    std::cout << "WASAPI renderer lifecycle tests passed\n";
    return 0;
}
