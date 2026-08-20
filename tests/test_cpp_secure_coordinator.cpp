#include "wabridge_messages.h"
#include "wabridge_secure_coordinator.h"
#include "wabridge_socket.h"
#include "wabridge_tls.h"
#include "wabridge_tls_stream.h"

#include <cstdlib>
#include <chrono>
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
    const auto server_id = make_identity("Secure Coordinator Server");
    const auto client_id = make_identity("Secure Coordinator Client");
    auto server_context = wabridge::tls::Context::server(server_id.certificate, server_id.private_key, client_id.certificate);
    auto client_context = wabridge::tls::Context::client(client_id.certificate, client_id.private_key, server_id.certificate);

    wabridge::messages::SessionHello server_hello;
    server_hello.role = wabridge::messages::Role::Windows;
    server_hello.session_nonce = wabridge::messages::fresh_session_nonce();
    server_hello.device_id = "secure-coordinator";
    server_hello.max_frame = 4 * 1024 * 1024;
    server_hello.capabilities_hash.fill(0xCD);
    wabridge::coordinator::SecureCoordinator coordinator(std::move(server_context), server_hello);
    REQUIRE(coordinator.start(0));
    const auto port = coordinator.port();
    REQUIRE(port != 0);

    auto connected = wabridge::net::Socket::connect("127.0.0.1", port);
    REQUIRE(connected.has_value());
    SSL* client_ssl = SSL_new(client_context.native());
    REQUIRE(client_ssl != nullptr);
    wabridge::tls::Stream client_stream(client_ssl, connected->native(), wabridge::tls::StreamMode::Client);
    wabridge::session::SessionPeer client_peer(client_stream, wabridge::messages::Role::Android, wabridge::messages::Role::Windows);
    wabridge::messages::SessionHello client_hello;
    client_hello.role = wabridge::messages::Role::Android;
    client_hello.session_nonce = wabridge::messages::fresh_session_nonce();
    client_hello.device_id = "secure-client";
    client_hello.max_frame = 4 * 1024 * 1024;
    client_hello.capabilities_hash.fill(0xAB);
    REQUIRE(client_peer.establish(client_hello));

    for (int i = 0; i < 100 && coordinator.established_sessions() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(coordinator.established_sessions() == 1);
    REQUIRE(coordinator.has_established_session());
    REQUIRE(coordinator.send({1, wabridge::messages::kHeartbeatAck, 0, 88, {1}}));
    const auto coordinator_frame = client_stream.read();
    REQUIRE(coordinator_frame.has_value());
    REQUIRE(coordinator_frame->kind == wabridge::messages::kHeartbeatAck);
    REQUIRE(coordinator_frame->request_id == 88);
    REQUIRE(client_peer.send({1, wabridge::messages::kHeartbeat, 0, 77, {1}}));
    const auto heartbeat_ack = client_stream.read();
    REQUIRE(heartbeat_ack.has_value());
    REQUIRE(heartbeat_ack->channel == 1);
    REQUIRE(heartbeat_ack->kind == wabridge::messages::kHeartbeatAck);
    REQUIRE(heartbeat_ack->request_id == 77);
    REQUIRE(heartbeat_ack->payload == std::vector<std::uint8_t>{1});
    client_peer.stop();
    for (int i = 0; i < 100 && coordinator.has_established_session(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(!coordinator.has_established_session());
    coordinator.stop();
    coordinator.stop();
    std::cout << "Secure coordinator integration tests passed\n";
}
