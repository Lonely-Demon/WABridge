#include "wabridge_identity.h"

#include "wabridge_tls.h"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace wabridge::identity {
namespace {

template <typename T, void (*Deleter)(T*)>
using openssl_ptr = std::unique_ptr<T, std::integral_constant<decltype(Deleter), Deleter>>;
using evp_key_ptr = openssl_ptr<EVP_PKEY, EVP_PKEY_free>;
using x509_ptr = openssl_ptr<X509, X509_free>;
struct BioDeleter {
    void operator()(BIO* bio) const noexcept {
        if (bio != nullptr) BIO_free(bio);
    }
};
using bio_ptr = std::unique_ptr<BIO, BioDeleter>;

std::string bio_string(BIO* bio) {
    BUF_MEM* memory = nullptr;
    BIO_get_mem_ptr(bio, &memory);
    if (memory == nullptr || memory->data == nullptr) throw std::runtime_error("failed to serialize identity");
    return {memory->data, memory->length};
}

std::string protect_key(const std::string& plain) {
#ifdef _WIN32
    DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"WABridge device key", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_LOCAL_MACHINE, &output)) {
        throw std::runtime_error("DPAPI could not protect the WABridge private key");
    }
    std::string result(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    return plain;
#endif
}

std::string unprotect_key(const std::string& protected_value) {
#ifdef _WIN32
    DATA_BLOB input{static_cast<DWORD>(protected_value.size()),
                    reinterpret_cast<BYTE*>(const_cast<char*>(protected_value.data()))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        throw std::runtime_error("DPAPI could not unprotect the WABridge private key");
    }
    std::string result(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    return protected_value;
#endif
}

std::string fingerprint_from_pem(const std::string& certificate_pem) {
    bio_ptr certificate_bio(BIO_new_mem_buf(certificate_pem.data(), static_cast<int>(certificate_pem.size())));
    if (!certificate_bio) throw std::runtime_error("could not read identity certificate");
    x509_ptr certificate(PEM_read_bio_X509(certificate_bio.get(), nullptr, nullptr, nullptr));
    if (!certificate) throw std::runtime_error("stored WABridge certificate is invalid");
    return tls::certificate_fingerprint_sha256(certificate.get());
}

Material generate_material() {
    evp_key_ptr key(EVP_EC_gen("prime256v1"));
    if (!key) throw std::runtime_error("could not generate P-256 identity key");

    x509_ptr certificate(X509_new());
    if (!certificate) throw std::runtime_error("could not create identity certificate");
    X509_set_version(certificate.get(), 2);
    ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1);
    X509_gmtime_adj(X509_get_notBefore(certificate.get()), 0);
    X509_gmtime_adj(X509_get_notAfter(certificate.get()), 365LL * 24LL * 60LL * 60LL);
    X509_set_pubkey(certificate.get(), key.get());
    auto* name = X509_get_subject_name(certificate.get());
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("WABridge device"), -1, -1, 0);
    X509_set_issuer_name(certificate.get(), name);
    if (X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0) {
        throw std::runtime_error("could not sign identity certificate");
    }

    bio_ptr certificate_bio(BIO_new(BIO_s_mem()));
    bio_ptr key_bio(BIO_new(BIO_s_mem()));
    if (!certificate_bio || !key_bio || PEM_write_bio_X509(certificate_bio.get(), certificate.get()) != 1 ||
        PEM_write_bio_PrivateKey(key_bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        throw std::runtime_error("could not serialize identity material");
    }
    Material material;
    material.certificate_pem = bio_string(certificate_bio.get());
    material.private_key_pem = bio_string(key_bio.get());
    material.fingerprint = tls::certificate_fingerprint_sha256(certificate.get());
    return material;
}

} // namespace

Store::Store(std::filesystem::path directory) : directory_(std::move(directory)) {
    if (directory_.empty()) {
#ifdef _WIN32
        wchar_t path[MAX_PATH]{};
        const auto length = GetEnvironmentVariableW(L"APPDATA", path, MAX_PATH);
        if (length > 0 && length < MAX_PATH) directory_ = std::filesystem::path(path) / "WABridge";
        else directory_ = std::filesystem::temp_directory_path() / "WABridge";
#else
        directory_ = std::filesystem::temp_directory_path() / "WABridge";
#endif
    }
}

Material Store::load_or_create() {
    std::filesystem::create_directories(directory_);
    const auto certificate_path = directory_ / "device-cert.pem";
    const auto key_path = directory_ / "device-key.dpapi";
    if (std::filesystem::exists(certificate_path) && std::filesystem::exists(key_path)) {
        std::ifstream certificate_file(certificate_path, std::ios::binary);
        std::ifstream key_file(key_path, std::ios::binary);
        Material material;
        material.certificate_pem.assign(std::istreambuf_iterator<char>(certificate_file), {});
        std::string protected_key(std::istreambuf_iterator<char>(key_file), {});
        material.private_key_pem = unprotect_key(protected_key);
        if (material.certificate_pem.empty() || material.private_key_pem.empty()) {
            throw std::runtime_error("stored WABridge identity is empty");
        }
        material.fingerprint = fingerprint_from_pem(material.certificate_pem);
        return material;
    }

    const auto material = generate_material();
    std::ofstream certificate_file(certificate_path, std::ios::binary | std::ios::trunc);
    std::ofstream key_file(key_path, std::ios::binary | std::ios::trunc);
    if (!certificate_file || !key_file) throw std::runtime_error("could not create WABridge identity files");
    certificate_file << material.certificate_pem;
    key_file << protect_key(material.private_key_pem);
    return material;
}

} // namespace wabridge::identity
