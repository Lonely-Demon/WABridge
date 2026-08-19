#include "wabridge_tls_stream.h"

#include <algorithm>
#include <cstring>

namespace wabridge::tls {

Stream::Stream(SSL* ssl, const net::NativeSocket socket, const StreamMode mode) : ssl_(ssl) {
    if (ssl_ == nullptr || socket == net::kInvalidSocket) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        throw TlsError("invalid TLS socket stream");
    }
#ifdef _WIN32
    BIO* bio = BIO_new_socket(static_cast<int>(socket), BIO_NOCLOSE);
#else
    BIO* bio = BIO_new_socket(socket, BIO_NOCLOSE);
#endif
    if (bio == nullptr) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        throw TlsError("unable to create TLS socket BIO");
    }
    SSL_set_bio(ssl_, bio, bio);
    if (mode == StreamMode::Server) {
        SSL_set_accept_state(ssl_);
    } else {
        SSL_set_connect_state(ssl_);
    }
}

namespace {

std::uint16_t read_u16(const unsigned char* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) | data[1]);
}

std::uint32_t read_u32(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

bool retryable_ssl_error(SSL* ssl, const int result) {
    const int error = SSL_get_error(ssl, result);
    return error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE;
}

} // namespace

Stream::~Stream() {
    close();
}

bool Stream::handshake() {
    if (ssl_ == nullptr || closed_) return false;
    for (;;) {
        const int result = SSL_do_handshake(ssl_);
        if (result == 1) {
            X509* peer = SSL_get_peer_certificate(ssl_);
            const bool valid = negotiated_tls13(ssl_) && peer != nullptr;
            X509_free(peer);
            return valid;
        }
        if (!retryable_ssl_error(ssl_, result)) {
            close();
            return false;
        }
    }
}

bool Stream::write(const protocol::Envelope& envelope) {
    if (ssl_ == nullptr || closed_) return false;
    std::vector<std::uint8_t> frame;
    try {
        frame = protocol::encode(envelope);
    } catch (const protocol::ProtocolError&) {
        return false;
    }
    return write_exact(frame.data(), frame.size());
}

std::optional<protocol::Envelope> Stream::read() {
    if (ssl_ == nullptr || closed_) return std::nullopt;

    unsigned char header[protocol::kHeaderSize]{};
    if (!read_exact(header, sizeof(header))) {
        return std::nullopt;
    }
    if (read_u16(header) != protocol::kMagic || header[2] != protocol::kVersion) {
        close();
        return std::nullopt;
    }
    const auto channel = header[3];
    std::size_t limit = 0;
    try {
        limit = protocol::channel_limit(channel);
    } catch (const protocol::ProtocolError&) {
        close();
        return std::nullopt;
    }
    const auto length = read_u32(header + 16);
    if (length == 0 || length > limit) {
        close();
        return std::nullopt;
    }

    std::vector<std::uint8_t> frame(protocol::kHeaderSize + static_cast<std::size_t>(length));
    std::copy(std::begin(header), std::end(header), frame.begin());
    if (!read_exact(frame.data() + protocol::kHeaderSize, length)) {
        return std::nullopt;
    }
    try {
        return protocol::decode(frame);
    } catch (const protocol::ProtocolError&) {
        close();
        return std::nullopt;
    }
}

bool Stream::read_exact(unsigned char* destination, const std::size_t length) {
    std::size_t offset = 0;
    while (offset < length) {
        const int result = SSL_read(ssl_, destination + offset,
                                    static_cast<int>(std::min<std::size_t>(length - offset, 1u << 20)));
        if (result > 0) {
            offset += static_cast<std::size_t>(result);
            continue;
        }
        if (retryable_ssl_error(ssl_, result)) continue;
        close();
        return false;
    }
    return true;
}

bool Stream::write_exact(const unsigned char* source, const std::size_t length) {
    std::size_t offset = 0;
    while (offset < length) {
        const int result = SSL_write(ssl_, source + offset,
                                     static_cast<int>(std::min<std::size_t>(length - offset, 1u << 20)));
        if (result > 0) {
            offset += static_cast<std::size_t>(result);
            continue;
        }
        if (retryable_ssl_error(ssl_, result)) continue;
        close();
        return false;
    }
    return true;
}

void Stream::close() noexcept {
    if (closed_) return;
    closed_ = true;
    if (ssl_ != nullptr) {
        (void)SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
}

} // namespace wabridge::tls
