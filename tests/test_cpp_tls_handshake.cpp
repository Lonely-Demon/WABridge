#include "wabridge_tls.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

#define REQUIRE(condition) do { if (!(condition)) std::abort(); } while (false)
#include <memory>
#include <string>

#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace {

struct Identity {
    std::string certificate;
    std::string private_key;
};

std::string bio_string(BIO* bio) {
    char* data = nullptr;
    const auto size = BIO_get_mem_data(bio, &data);
    return std::string(data, static_cast<std::size_t>(size));
}

Identity make_identity(const char* common_name) {
    EVP_PKEY* raw_key = EVP_RSA_gen(2048);
    REQUIRE(raw_key != nullptr);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(raw_key, EVP_PKEY_free);

    X509* raw_certificate = X509_new();
    REQUIRE(raw_certificate != nullptr);
    std::unique_ptr<X509, decltype(&X509_free)> certificate(raw_certificate, X509_free);
    REQUIRE(X509_set_version(certificate.get(), 2) == 1);
    REQUIRE(ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1) == 1);
    REQUIRE(X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0) != nullptr);
    REQUIRE(X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 3600) != nullptr);
    REQUIRE(X509_set_pubkey(certificate.get(), key.get()) == 1);

    X509_NAME* subject = X509_get_subject_name(certificate.get());
    REQUIRE(X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(common_name), -1, -1, 0) == 1);
    REQUIRE(X509_set_issuer_name(certificate.get(), subject) == 1);
    REQUIRE(X509_sign(certificate.get(), key.get(), EVP_sha256()) > 0);

    BIO* certificate_bio = BIO_new(BIO_s_mem());
    BIO* key_bio = BIO_new(BIO_s_mem());
    REQUIRE(certificate_bio != nullptr && key_bio != nullptr);
    REQUIRE(PEM_write_bio_X509(certificate_bio, certificate.get()) == 1);
    REQUIRE(PEM_write_bio_PrivateKey(key_bio, key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1);
    Identity result{bio_string(certificate_bio), bio_string(key_bio)};
    BIO_free(certificate_bio);
    BIO_free(key_bio);
    return result;
}

bool handshake(SSL* client, SSL* server) {
    BIO* client_bio = nullptr;
    BIO* server_bio = nullptr;
    REQUIRE(BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) == 1);
    SSL_set_bio(client, client_bio, client_bio);
    SSL_set_bio(server, server_bio, server_bio);
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);

    bool client_done = false;
    bool server_done = false;
    for (int attempt = 0; attempt < 100 && (!client_done || !server_done); ++attempt) {
        if (!client_done) {
            const int result = SSL_do_handshake(client);
            if (result == 1) {
                client_done = true;
            } else {
                const int error = SSL_get_error(client, result);
                if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
                    return false;
                }
            }
        }
        if (!server_done) {
            const int result = SSL_do_handshake(server);
            if (result == 1) {
                server_done = true;
            } else {
                const int error = SSL_get_error(server, result);
                if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
                    return false;
                }
            }
        }
    }
    return client_done && server_done;
}

} // namespace

int main() {
    const auto server_identity = make_identity("WABridge Test Server");
    const auto client_identity = make_identity("WABridge Test Client");
    REQUIRE(server_identity.certificate.find("BEGIN CERTIFICATE") != std::string::npos);
    REQUIRE(client_identity.certificate.find("BEGIN CERTIFICATE") != std::string::npos);
    REQUIRE(server_identity.private_key.find("BEGIN") != std::string::npos);
    REQUIRE(client_identity.private_key.find("BEGIN") != std::string::npos);
    const auto server_context = wabridge::tls::Context::server(
        server_identity.certificate, server_identity.private_key, client_identity.certificate);
    const auto client_context = wabridge::tls::Context::client(
        client_identity.certificate, client_identity.private_key, server_identity.certificate);

    SSL* server = SSL_new(server_context.native());
    SSL* client = SSL_new(client_context.native());
    REQUIRE(server != nullptr && client != nullptr);
    REQUIRE(handshake(client, server));

    REQUIRE(wabridge::tls::negotiated_tls13(client));
    REQUIRE(wabridge::tls::negotiated_tls13(server));
    REQUIRE(wabridge::tls::peer_fingerprint_sha256(client) ==
           wabridge::tls::certificate_fingerprint_sha256(SSL_get_peer_certificate(client)));
    REQUIRE(wabridge::tls::peer_fingerprint_sha256(server) ==
           wabridge::tls::certificate_fingerprint_sha256(SSL_get_peer_certificate(server)));

    SSL_free(client);
    SSL_free(server);

    const auto wrong_client_context = wabridge::tls::Context::client(
        client_identity.certificate, client_identity.private_key, client_identity.certificate);
    const auto pinned_server_context = wabridge::tls::Context::server(
        server_identity.certificate, server_identity.private_key, client_identity.certificate);
    SSL* wrong_client = SSL_new(wrong_client_context.native());
    SSL* pinned_server = SSL_new(pinned_server_context.native());
    REQUIRE(wrong_client != nullptr && pinned_server != nullptr);
    REQUIRE(!handshake(wrong_client, pinned_server));
    SSL_free(wrong_client);
    SSL_free(pinned_server);

    std::cout << "TLS 1.3 bilateral pinning handshake and wrong-pin rejection passed\\n";
    return 0;
}
