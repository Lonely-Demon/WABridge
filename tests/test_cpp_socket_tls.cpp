#include "wabridge_messages.h"
#include "wabridge_socket.h"
#include "wabridge_tls.h"
#include "wabridge_tls_stream.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#define REQUIRE(condition) do { if (!(condition)) { std::cerr << "require failed: " << #condition << "\\n"; std::abort(); } } while (false)

namespace {

struct Identity { std::string certificate; std::string private_key; };

std::string bio_string(BIO* bio) {
    char* data = nullptr;
    const auto size = BIO_get_mem_data(bio, &data);
    return std::string(data, static_cast<std::size_t>(size));
}

Identity make_identity(const char* common_name) {
    EVP_PKEY* key = EVP_RSA_gen(2048);
    REQUIRE(key != nullptr);
    X509* certificate = X509_new();
    REQUIRE(certificate != nullptr);
    REQUIRE(X509_set_version(certificate, 2) == 1);
    REQUIRE(ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1) == 1);
    REQUIRE(X509_gmtime_adj(X509_getm_notBefore(certificate), 0) != nullptr);
    REQUIRE(X509_gmtime_adj(X509_getm_notAfter(certificate), 3600) != nullptr);
    REQUIRE(X509_set_pubkey(certificate, key) == 1);
    X509_NAME* subject = X509_get_subject_name(certificate);
    REQUIRE(X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
                                       reinterpret_cast<const unsigned char*>(common_name), -1, -1, 0) == 1);
    REQUIRE(X509_set_issuer_name(certificate, subject) == 1);
    REQUIRE(X509_sign(certificate, key, EVP_sha256()) > 0);

    BIO* certificate_bio = BIO_new(BIO_s_mem());
    BIO* key_bio = BIO_new(BIO_s_mem());
    REQUIRE(certificate_bio != nullptr && key_bio != nullptr);
    REQUIRE(PEM_write_bio_X509(certificate_bio, certificate) == 1);
    REQUIRE(PEM_write_bio_PrivateKey(key_bio, key, nullptr, nullptr, 0, nullptr, nullptr) == 1);
    Identity result{bio_string(certificate_bio), bio_string(key_bio)};
    BIO_free(certificate_bio);
    BIO_free(key_bio);
    X509_free(certificate);
    EVP_PKEY_free(key);
    return result;
}

} // namespace

int main() {
    wabridge::net::Runtime runtime;
    const auto server_identity = make_identity("WABridge Loopback Server");
    const auto client_identity = make_identity("WABridge Loopback Client");
    const auto server_context = wabridge::tls::Context::server(
        server_identity.certificate, server_identity.private_key, client_identity.certificate);
    const auto client_context = wabridge::tls::Context::client(
        client_identity.certificate, client_identity.private_key, server_identity.certificate);

    auto listener = wabridge::net::Socket::listen(0);
    REQUIRE(listener.has_value());
    const auto port = listener->local_port();
    REQUIRE(port != 0);

    bool server_ok = false;
    std::string server_error;
    std::thread server_thread([&] {
        try {
            auto accepted = listener->accept();
            if (!accepted.has_value()) { server_error = "accept"; return; }
            SSL* ssl = SSL_new(server_context.native());
            if (ssl == nullptr) { server_error = "SSL_new"; return; }
            wabridge::tls::Stream stream(ssl, accepted->native(), wabridge::tls::StreamMode::Server);
            if (!stream.handshake()) { ERR_print_errors_fp(stderr); server_error = "server handshake"; return; }
            const auto incoming = stream.read();
            if (!incoming.has_value() || incoming->channel != 1 || incoming->kind != 1) { server_error = "server read hello"; return; }
            const auto hello = wabridge::messages::decode_session_hello(incoming->payload);
            if (hello.role != wabridge::messages::Role::Android) { server_error = "wrong client role"; return; }
            wabridge::messages::SessionHello response;
            response.role = wabridge::messages::Role::Windows;
            response.session_nonce = wabridge::messages::fresh_session_nonce();
            response.device_id = "loopback-server";
            response.max_frame = 4 * 1024 * 1024;
            response.capabilities_hash.fill(0xCD);
            server_ok = stream.write({1, 1, 1, 2, wabridge::messages::encode_session_hello(response)});
        } catch (...) {
            server_ok = false;
        }
    });

    auto connected = wabridge::net::Socket::connect("127.0.0.1", port);
    REQUIRE(connected.has_value());
    SSL* client_ssl = SSL_new(client_context.native());
    REQUIRE(client_ssl != nullptr);
    wabridge::tls::Stream client(client_ssl, connected->native(), wabridge::tls::StreamMode::Client);
    if (!client.handshake()) {
        server_thread.join();
        ERR_print_errors_fp(stderr);
        std::cerr << "client handshake failed; server stage=" << server_error << "\\n";
        std::abort();
    }

    wabridge::messages::SessionHello request;
    request.role = wabridge::messages::Role::Android;
    request.session_nonce = wabridge::messages::fresh_session_nonce();
    request.device_id = "loopback-client";
    request.max_frame = 4 * 1024 * 1024;
    request.capabilities_hash.fill(0xAB);
    REQUIRE(client.write({1, 1, 1, 1, wabridge::messages::encode_session_hello(request)}));
    const auto response = client.read();
    REQUIRE(response.has_value());
    REQUIRE(wabridge::messages::decode_session_hello(response->payload).role ==
            wabridge::messages::Role::Windows);

    server_thread.join();
    REQUIRE(server_ok);
    std::cout << "TCP + TLS 1.3 + SESSION_HELLO integration passed\n";
    return 0;
}
