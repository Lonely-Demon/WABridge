#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace wabridge::net {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;
#endif

class Runtime final {
public:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
private:
    bool initialized_{false};
};

class Socket final {
public:
    Socket() noexcept = default;
    explicit Socket(NativeSocket socket) noexcept : socket_(socket) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    ~Socket();

    static std::optional<Socket> connect(std::string_view host, std::uint16_t port);
    static std::optional<Socket> listen(std::uint16_t port);

    std::optional<Socket> accept() const;
    bool valid() const noexcept { return socket_ != kInvalidSocket; }
    NativeSocket native() const noexcept { return socket_; }
    std::uint16_t local_port() const noexcept;
    void close() noexcept;

private:
    NativeSocket socket_{kInvalidSocket};
};

} // namespace wabridge::net
