#include "test_require.h"
#include "wabridge_socket.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

int main() {
    wabridge::net::Runtime runtime;
    auto listener = wabridge::net::Socket::listen(0);
    REQUIRE(listener.has_value());
    const auto port = listener->local_port();
    REQUIRE(port != 0);

    bool accepted = false;
    std::thread server([&] {
        auto peer = listener->accept();
        REQUIRE(peer.has_value());
        const char expected[] = "wab";
        char received[sizeof(expected)]{};
#ifdef _WIN32
        const int result = recv(peer->native(), received, sizeof(expected), 0);
#else
        const auto result = recv(peer->native(), received, sizeof(expected), 0);
#endif
        REQUIRE(result == static_cast<int>(sizeof(expected)));
        REQUIRE(std::memcmp(received, expected, sizeof(expected)) == 0);
        accepted = true;
    });

    auto client = wabridge::net::Socket::connect("127.0.0.1", port);
    REQUIRE(client.has_value());
    const char payload[] = "wab";
#ifdef _WIN32
    REQUIRE(send(client->native(), payload, sizeof(payload), 0) == static_cast<int>(sizeof(payload)));
#else
    REQUIRE(send(client->native(), payload, sizeof(payload), 0) == static_cast<ssize_t>(sizeof(payload)));
#endif
    client->close();
    client->close();
    server.join();
    REQUIRE(accepted);

    std::cout << "Socket loopback tests passed\n";
    return 0;
}
