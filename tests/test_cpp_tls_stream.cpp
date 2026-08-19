#include "wabridge_tls_stream.h"

#include <cassert>
#include <iostream>
#include <string>
#include <thread>

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
    assert(raw_key != nullptr);
    X509* raw_certificate = X509_new();
    assert(raw_certificate != nullptr);
    assert(X509_set_version(raw_certificate, 2) == 1);
    assert(ASN1_INTEGER_set(X509_get_serialNumber(raw_certificate), 1) == 1);
    assert(X509_gmtime_adj(X509_getm_notBefore(raw_certificate), 0) != nullptr);
    assert(X509_gmtime_adj(X509_getm_notAfter(raw_certificate), 3600) != nullptr);
    assert(X509_set_pubkey(raw_certificate, raw_key) == 1);
    X509_NAME* subject = X509_get_subject_name(raw_certificate);
    assert(X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(common_name), -1, -1, 0) == 1);
    assert(X509_set_issuer_name(raw_certificate, subject) == 1);
    assert(X509_sign(raw_certificate, raw_key, EVP_sha256()) > 0);

    BIO* certificate_bio = BIO_new(BIO_s_mem());
    BIO* key_bio = BIO_new(BIO_s_mem());
    assert(certificate_bio != nullptr && key_bio != nullptr);
    assert(PEM_write_bio_X509(certificate_bio, raw_certificate) == 1);
    assert(PEM_write_bio_PrivateKey(key_bio, raw_key, nullptr, nullptr, 0, nullptr, nullptr) == 1);
    Identity result{bio_string(certificate_bio), bio_string(key_bio)};
    BIO_free(certificate_bio);
    BIO_free(key_bio);
    X509_free(raw_certificate);
    EVP_PKEY_free(raw_key);
    return result;
}

} // namespace

int main() {
    const auto server_identity = make_identity("WABridge Stream Server");
    const auto client_identity = make_identity("WABridge Stream Client");
    const auto server_context = wabridge::tls::Context::server(
        server_identity.certificate, server_identity.private_key, client_identity.certificate);
    const auto client_context = wabridge::tls::Context::client(
        client_identity.certificate, client_identity.private_key, server_identity.certificate);

    SSL* server_ssl = SSL_new(server_context.native());
    SSL* client_ssl = SSL_new(client_context.native());
    assert(server_ssl != nullptr && client_ssl != nullptr);
    BIO* client_bio = nullptr;
    BIO* server_bio = nullptr;
    assert(BIO_new_bio_pair(&client_bio, 0, &server_bio, 0) == 1);
    SSL_set_bio(client_ssl, client_bio, client_bio);
    SSL_set_bio(server_ssl, server_bio, server_bio);
    SSL_set_connect_state(client_ssl);
    SSL_set_accept_state(server_ssl);

    wabridge::tls::Stream client(client_ssl);
    wabridge::tls::Stream server(server_ssl);
    bool client_handshake = false;
    bool server_handshake = false;
    std::thread client_thread([&] { client_handshake = client.handshake(); });
    std::thread server_thread([&] { server_handshake = server.handshake(); });
    client_thread.join();
    server_thread.join();
    assert(client_handshake && server_handshake);

    wabridge::protocol::Envelope outgoing;
    outgoing.channel = 1;
    outgoing.kind = 0x0005;
    outgoing.flags = 1;
    outgoing.request_id = 99;
    outgoing.payload.assign(4096, static_cast<std::uint8_t>('x'));

    bool write_ok = false;
    std::thread writer([&] { write_ok = client.write(outgoing); });
    const auto incoming = server.read();
    writer.join();
    assert(write_ok);
    assert(incoming.has_value());
    assert(incoming.value() == outgoing);

    std::cout << "TLS stream handshake and envelope transport passed\n";
    return 0;
}
