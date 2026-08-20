#include "test_require.h"
#include "wabridge_input_capture.h"

#include <iostream>

int main() {
    bool seen = false;
    wabridge::platform_input::LowLevelCapture capture(
        [&](const wabridge::input::Event&) { seen = true; });
    REQUIRE(!capture.active());
#ifndef _WIN32
    REQUIRE(!capture.start());
#endif
    capture.stop();
    capture.stop();
    REQUIRE(!capture.active());
    REQUIRE(!seen);
    std::cout << "Input-capture lifecycle tests passed\n";
    return 0;
}
