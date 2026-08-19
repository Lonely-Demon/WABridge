#pragma once

#include "wabridge_envelope.h"
#include "wabridge_tls.h"
#include "wabridge_socket.h"

#include <memory>
#include <optional>
#include <vector>

namespace wabridge::tls {

enum class StreamMode { Client, Server };

class Stream final {
public:
    // Takes ownership of the SSL object. The caller must configure a connected
    // socket with SSL_set_fd before constructing the stream.
    explicit Stream(SSL* ssl) noexcept : ssl_(ssl) {}
    Stream(SSL* ssl, net::NativeSocket socket, StreamMode mode);
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    Stream(Stream&&) = delete;
    Stream& operator=(Stream&&) = delete;
    ~Stream();

    bool handshake();
    bool write(const protocol::Envelope& envelope);
    std::optional<protocol::Envelope> read();
    void close() noexcept;

    SSL* native() const noexcept { return ssl_; }

private:
    bool read_exact(unsigned char* destination, std::size_t length);
    bool write_exact(const unsigned char* source, std::size_t length);

    SSL* ssl_{};
    bool closed_{false};
};

} // namespace wabridge::tls
