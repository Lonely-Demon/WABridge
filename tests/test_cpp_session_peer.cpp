#include "wabridge_messages.h"
#include "wabridge_session_peer.h"
#include "wabridge_socket.h"
#include "wabridge_tls.h"
#include "wabridge_tls_stream.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <openssl/pem.h>
#include <openssl/rsa.h>

#define REQUIRE(condition) do { if (!(condition)) std::abort(); } while (false)

namespace {
struct Identity { std::string certificate; std::string private_key; };
std::string bio_string(BIO* bio) { char* data = nullptr; const auto size = BIO_get_mem_data(bio, &data); return {data, static_cast<std::size_t>(size)}; }
Identity make_identity(const char* name) {
    EVP_PKEY* key = EVP_RSA_gen(2048); REQUIRE(key != nullptr);
    X509* cert = X509_new(); REQUIRE(cert != nullptr);
    REQUIRE(X509_set_version(cert, 2) == 1);
    REQUIRE(ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) == 1);
    REQUIRE(X509_gmtime_adj(X509_getm_notBefore(cert), 0) != nullptr);
    REQUIRE(X509_gmtime_adj(X509_getm_notAfter(cert), 3600) != nullptr);
    REQUIRE(X509_set_pubkey(cert, key) == 1);
    X509_NAME* subject = X509_get_subject_name(cert);
    REQUIRE(X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(name), -1, -1, 0) == 1);
    REQUIRE(X509_set_issuer_name(cert, subject) == 1);
    REQUIRE(X509_sign(cert, key, EVP_sha256()) > 0);
    BIO* cert_bio = BIO_new(BIO_s_mem()); BIO* key_bio = BIO_new(BIO_s_mem());
    REQUIRE(cert_bio != nullptr && key_bio != nullptr);
    REQUIRE(PEM_write_bio_X509(cert_bio, cert) == 1);
    REQUIRE(PEM_write_bio_PrivateKey(key_bio, key, nullptr, nullptr, 0, nullptr, nullptr) == 1);
    Identity result{bio_string(cert_bio), bio_string(key_bio)};
    BIO_free(cert_bio); BIO_free(key_bio); X509_free(cert); EVP_PKEY_free(key);
    return result;
}
}

int main() {
    wabridge::net::Runtime runtime;
    const auto server_id = make_identity("SessionPeer Server");
    const auto client_id = make_identity("SessionPeer Client");
    const auto server_ctx = wabridge::tls::Context::server(server_id.certificate, server_id.private_key, client_id.certificate);
    const auto client_ctx = wabridge::tls::Context::client(client_id.certificate, client_id.private_key, server_id.certificate);
    auto listener = wabridge::net::Socket::listen(0); REQUIRE(listener.has_value());
    const auto port = listener->local_port(); REQUIRE(port != 0);
    bool server_ok = false;
    std::thread server_thread([&] {
        auto accepted = listener->accept(); if (!accepted) return;
        SSL* ssl = SSL_new(server_ctx.native()); if (!ssl) return;
        wabridge::tls::Stream stream(ssl, accepted->native(), wabridge::tls::StreamMode::Server);
        wabridge::session::SessionPeer peer(stream, wabridge::messages::Role::Windows, wabridge::messages::Role::Android);
        wabridge::messages::SessionHello hello;
        hello.role = wabridge::messages::Role::Windows;
        hello.session_nonce = wabridge::messages::fresh_session_nonce();
        hello.device_id = "peer-server"; hello.max_frame = 4 * 1024 * 1024; hello.capabilities_hash.fill(0xCD);
        server_ok = peer.establish(hello) && peer.peer_hello().has_value();
    });
    auto connected = wabridge::net::Socket::connect("127.0.0.1", port); REQUIRE(connected.has_value());
    SSL* client_ssl = SSL_new(client_ctx.native()); REQUIRE(client_ssl != nullptr);
    wabridge::tls::Stream client_stream(client_ssl, connected->native(), wabridge::tls::StreamMode::Client);
    wabridge::session::SessionPeer client_peer(client_stream, wabridge::messages::Role::Android, wabridge::messages::Role::Windows);
    wabridge::messages::SessionHello client_hello;
    client_hello.role = wabridge::messages::Role::Android;
    client_hello.session_nonce = wabridge::messages::fresh_session_nonce();
    client_hello.device_id = "peer-client"; client_hello.max_frame = 4 * 1024 * 1024; client_hello.capabilities_hash.fill(0xAB);
    REQUIRE(client_peer.establish(client_hello));
    REQUIRE(client_peer.peer_hello()->role == wabridge::messages::Role::Windows);
    client_peer.stop(); client_peer.stop();
    server_thread.join(); REQUIRE(server_ok);
    std::cout << "SessionPeer integration tests passed\n";
}
