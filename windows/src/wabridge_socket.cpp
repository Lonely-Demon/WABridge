#include "wabridge_socket.h"

#include <cstring>
#include <string>
#include <utility>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace wabridge::net {
namespace {

void close_native(const NativeSocket socket) noexcept {
    if (socket == kInvalidSocket) return;
#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

void set_reuse_address(const NativeSocket socket) {
    int enabled = 1;
    (void)setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
                     reinterpret_cast<const char*>(&enabled),
#else
                     &enabled,
#endif
                     sizeof(enabled));
}

} // namespace

Runtime::Runtime() {
#ifdef _WIN32
    WSADATA data{};
    initialized_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    initialized_ = true;
#endif
}

Runtime::~Runtime() {
#ifdef _WIN32
    if (initialized_) WSACleanup();
#endif
}

Socket::Socket(Socket&& other) noexcept : socket_(std::exchange(other.socket_, kInvalidSocket)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = std::exchange(other.socket_, kInvalidSocket);
    }
    return *this;
}

Socket::~Socket() {
    close();
}

std::optional<Socket> Socket::connect(const std::string_view host, const std::uint16_t port) {
    if (host.empty() || port == 0) return std::nullopt;
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;
    const auto port_text = std::to_string(port);
    addrinfo* results = nullptr;
    if (getaddrinfo(std::string(host).c_str(), port_text.c_str(), &hints, &results) != 0) {
        return std::nullopt;
    }

    Socket result;
    for (auto* current = results; current != nullptr; current = current->ai_next) {
        const auto candidate = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (candidate == kInvalidSocket) continue;
        if (::connect(candidate, current->ai_addr, static_cast<int>(current->ai_addrlen)) == 0) {
            result.socket_ = candidate;
            break;
        }
        close_native(candidate);
    }
    freeaddrinfo(results);
    if (!result.valid()) return std::nullopt;
    return result;
}

std::optional<Socket> Socket::listen(const std::uint16_t port) {
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET6;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;
    const auto port_text = std::to_string(port);
    addrinfo* results = nullptr;
    if (getaddrinfo(nullptr, port_text.c_str(), &hints, &results) != 0) return std::nullopt;

    Socket result;
    for (auto* current = results; current != nullptr; current = current->ai_next) {
        const auto candidate = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (candidate == kInvalidSocket) continue;
        set_reuse_address(candidate);
        if (::bind(candidate, current->ai_addr, static_cast<int>(current->ai_addrlen)) == 0 &&
            ::listen(candidate, 1) == 0) {
            result.socket_ = candidate;
            break;
        }
        close_native(candidate);
    }
    freeaddrinfo(results);
    if (!result.valid()) return std::nullopt;
    return result;
}

std::uint16_t Socket::local_port() const noexcept {
    if (!valid()) return 0;
    sockaddr_storage address{};
#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    if (getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &length) != 0) return 0;
    if (address.ss_family == AF_INET) {
        return ntohs(reinterpret_cast<const sockaddr_in*>(&address)->sin_port);
    }
    if (address.ss_family == AF_INET6) {
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&address)->sin6_port);
    }
    return 0;
}

std::optional<Socket> Socket::accept() const {
    if (!valid()) return std::nullopt;
    const auto accepted = ::accept(socket_, nullptr, nullptr);
    if (accepted == kInvalidSocket) return std::nullopt;
    return Socket(accepted);
}

void Socket::close() noexcept {
    if (!valid()) return;
    close_native(socket_);
    socket_ = kInvalidSocket;
}

} // namespace wabridge::net
