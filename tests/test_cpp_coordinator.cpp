#include "wabridge_coordinator.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    wabridge::net::Runtime runtime;
    std::atomic<bool> accepted{false};
    wabridge::coordinator::Coordinator coordinator([&](wabridge::net::Socket socket) {
        accepted.store(socket.valid());
    });

    assert(coordinator.start(0));
    const auto port = coordinator.port();
    assert(port != 0);
    assert(!coordinator.start(0));
    auto client = wabridge::net::Socket::connect("127.0.0.1", port);
    assert(client.has_value());
    for (int i = 0; i < 100 && !accepted.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(accepted.load());
    coordinator.stop();
    coordinator.stop();
    assert(!coordinator.running());
    assert(coordinator.port() == 0);

    std::cout << "Coordinator lifecycle tests passed\n";
    return 0;
}
