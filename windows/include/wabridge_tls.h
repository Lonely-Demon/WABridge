#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <openssl/ssl.h>
#include <openssl/x509.h>

namespace wabridge::tls {

class TlsError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Context final {
public:
    // Pairing mode intentionally permits an unauthenticated TLS peer, but the
    // caller must not authorize application capabilities until SAS approval.
    static Context client_pairing();
    static Context client(std::string_view own_certificate_pem,
                          std::string_view own_private_key_pem,
                          std::optional<std::string_view> pinned_peer_certificate_pem);
    static Context client_pinned(std::string_view peer_certificate_pem);
    static Context server(std::string_view own_certificate_pem,
                          std::string_view own_private_key_pem,
                          std::optional<std::string_view> pinned_peer_certificate_pem);

    Context(Context&&) noexcept;
    Context& operator=(Context&&) noexcept;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    ~Context();

    SSL_CTX* native() const noexcept { return context_; }

private:
    explicit Context(SSL_CTX* context) noexcept : context_(context) {}
    static Context make(bool server, bool verify_peer);
    static void load_certificate(SSL_CTX* context, std::string_view certificate_pem,
                                 bool own_certificate);
    static void load_private_key(SSL_CTX* context, std::string_view private_key_pem);
    static void add_pinned_certificate(SSL_CTX* context, std::string_view certificate_pem);

    SSL_CTX* context_{};
};

std::string certificate_fingerprint_sha256(X509* certificate);
std::string peer_fingerprint_sha256(SSL* ssl);
bool negotiated_tls13(SSL* ssl) noexcept;

} // namespace wabridge::tls
