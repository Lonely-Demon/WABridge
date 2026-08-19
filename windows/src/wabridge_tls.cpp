#include "wabridge_tls.h"

#include <iomanip>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <sstream>
#include <utility>

namespace wabridge::tls {
namespace {

std::string openssl_error(const char* operation) {
    std::ostringstream message;
    message << operation << ": ";
    unsigned long error = ERR_get_error();
    if (error == 0) {
        message << "unknown OpenSSL error";
    } else {
        char buffer[256]{};
        ERR_error_string_n(error, buffer, sizeof(buffer));
        message << buffer;
    }
    return message.str();
}

int accept_unpinned_peer(int, X509_STORE_CTX*) {
    // Used only during explicit first-pair mode. The application must compare
    // the peer fingerprint/SAS before authorizing any feature module.
    return 1;
}

X509* parse_certificate(std::string_view pem) {
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (bio == nullptr) {
        throw TlsError(openssl_error("BIO_new_mem_buf"));
    }
    X509* certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (certificate == nullptr) {
        throw TlsError(openssl_error("PEM_read_bio_X509"));
    }
    return certificate;
}

} // namespace

Context Context::make(const bool server, const bool verify_peer) {
    const auto method = TLS_method();
    SSL_CTX* context = SSL_CTX_new(method);
    if (context == nullptr) {
        throw TlsError(openssl_error("SSL_CTX_new"));
    }
    if (SSL_CTX_set_min_proto_version(context, TLS1_3_VERSION) != 1 ||
        SSL_CTX_set_max_proto_version(context, TLS1_3_VERSION) != 1) {
        SSL_CTX_free(context);
        throw TlsError(openssl_error("TLS 1.3 configuration"));
    }
    SSL_CTX_set_max_early_data(context, 0);
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_verify(context, verify_peer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);
    if (server) {
        SSL_CTX_set_session_cache_mode(context, SSL_SESS_CACHE_OFF);
    }
    return Context(context);
}

Context Context::client_pairing() {
    return make(false, false);
}

Context Context::client(const std::string_view own_certificate_pem,
                        const std::string_view own_private_key_pem,
                        const std::optional<std::string_view> pinned_peer_certificate_pem) {
    Context result = make(false, pinned_peer_certificate_pem.has_value());
    load_certificate(result.context_, own_certificate_pem, true);
    load_private_key(result.context_, own_private_key_pem);
    if (pinned_peer_certificate_pem.has_value()) {
        add_pinned_certificate(result.context_, pinned_peer_certificate_pem.value());
    }
    return result;
}

Context Context::client_pinned(const std::string_view peer_certificate_pem) {
    Context result = make(false, true);
    add_pinned_certificate(result.context_, peer_certificate_pem);
    return result;
}

Context Context::server(const std::string_view own_certificate_pem,
                        const std::string_view own_private_key_pem,
                        const std::optional<std::string_view> pinned_peer_certificate_pem) {
    Context result = make(true, true);
    if (pinned_peer_certificate_pem.has_value()) {
        SSL_CTX_set_verify(result.context_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    } else {
        SSL_CTX_set_verify(result.context_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                           accept_unpinned_peer);
    }
    load_certificate(result.context_, own_certificate_pem, true);
    load_private_key(result.context_, own_private_key_pem);
    if (pinned_peer_certificate_pem.has_value()) {
        add_pinned_certificate(result.context_, pinned_peer_certificate_pem.value());
    }
    return result;
}

void Context::load_certificate(SSL_CTX* context, const std::string_view certificate_pem,
                               const bool own_certificate) {
    BIO* bio = BIO_new_mem_buf(certificate_pem.data(), static_cast<int>(certificate_pem.size()));
    if (bio == nullptr) {
        throw TlsError(openssl_error("BIO_new_mem_buf"));
    }
    X509* certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (certificate == nullptr) {
        throw TlsError(openssl_error("PEM_read_bio_X509"));
    }
    const int result = own_certificate
        ? SSL_CTX_use_certificate(context, certificate)
        : X509_STORE_add_cert(SSL_CTX_get_cert_store(context), certificate);
    X509_free(certificate);
    if (result != 1) {
        throw TlsError(openssl_error("load certificate"));
    }
}

void Context::load_private_key(SSL_CTX* context, const std::string_view private_key_pem) {
    BIO* bio = BIO_new_mem_buf(private_key_pem.data(), static_cast<int>(private_key_pem.size()));
    if (bio == nullptr) {
        throw TlsError(openssl_error("BIO_new_mem_buf"));
    }
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (key == nullptr) {
        throw TlsError(openssl_error("PEM_read_bio_PrivateKey"));
    }
    const int result = SSL_CTX_use_PrivateKey(context, key);
    EVP_PKEY_free(key);
    if (result != 1 || SSL_CTX_check_private_key(context) != 1) {
        throw TlsError(openssl_error("load private key"));
    }
}

void Context::add_pinned_certificate(SSL_CTX* context, const std::string_view certificate_pem) {
    X509* certificate = parse_certificate(certificate_pem);
    X509_STORE* store = SSL_CTX_get_cert_store(context);
    const int result = X509_STORE_add_cert(store, certificate);
    X509_free(certificate);
    if (result != 1) {
        throw TlsError(openssl_error("add pinned certificate"));
    }
}

Context::Context(Context&& other) noexcept : context_(std::exchange(other.context_, nullptr)) {}

Context& Context::operator=(Context&& other) noexcept {
    if (this != &other) {
        SSL_CTX_free(context_);
        context_ = std::exchange(other.context_, nullptr);
    }
    return *this;
}

Context::~Context() {
    SSL_CTX_free(context_);
}

std::string certificate_fingerprint_sha256(X509* certificate) {
    if (certificate == nullptr) {
        throw TlsError("missing certificate");
    }
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digest_size = 0;
    if (X509_digest(certificate, EVP_sha256(), digest, &digest_size) != 1) {
        throw TlsError(openssl_error("X509_digest"));
    }
    std::ostringstream output;
    for (unsigned int i = 0; i < digest_size; ++i) {
        if (i != 0) output << ':';
        output << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(digest[i]);
    }
    return output.str();
}

std::string peer_fingerprint_sha256(SSL* ssl) {
    if (ssl == nullptr) {
        throw TlsError("missing SSL session");
    }
    X509* certificate = SSL_get_peer_certificate(ssl);
    if (certificate == nullptr) {
        throw TlsError("peer did not present a certificate");
    }
    const auto fingerprint = certificate_fingerprint_sha256(certificate);
    X509_free(certificate);
    return fingerprint;
}

bool negotiated_tls13(SSL* ssl) noexcept {
    return ssl != nullptr && SSL_version(ssl) == TLS1_3_VERSION;
}

} // namespace wabridge::tls
