#include "wabridge_pairing.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <openssl/evp.h>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace wabridge::pairing {
namespace {

std::string canonical(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isxdigit(character) != 0) {
            result.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return result;
}

std::string sha256_hex(std::string_view input) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        throw std::runtime_error("unable to allocate SHA-256 context");
    }
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digest_size = 0;
    const bool initialized = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;
    const bool updated = initialized && EVP_DigestUpdate(context, input.data(), input.size()) == 1;
    const bool finalized = updated && EVP_DigestFinal_ex(context, digest, &digest_size) == 1;
    EVP_MD_CTX_free(context);
    if (!finalized) {
        throw std::runtime_error("SHA-256 failed");
    }

    std::ostringstream output;
    for (unsigned int i = 0; i < digest_size; ++i) {
        output << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(digest[i]);
    }
    return output.str();
}

} // namespace

void Store::remember(Record record) {
    if (record.device_id.empty() || record.certificate_fingerprint.empty()) {
        throw std::invalid_argument("pairing record requires device id and fingerprint");
    }
    record.certificate_fingerprint = canonical(record.certificate_fingerprint);
    record_ = std::move(record);
}

void Store::revoke(const std::string_view device_id) {
    if (record_.has_value() && record_->device_id == device_id) {
        record_.reset();
    }
}

std::optional<Record> Store::find(const std::string_view device_id) const {
    if (record_.has_value() && record_->device_id == device_id) {
        return record_;
    }
    return std::nullopt;
}

State Store::evaluate(const std::string_view device_id,
                      const std::string_view peer_fingerprint) const {
    if (!record_.has_value() || record_->device_id != device_id) {
        return State::Unknown;
    }
    return canonical(peer_fingerprint) == record_->certificate_fingerprint
        ? State::Authenticated
        : State::BlockedChangedIdentity;
}

std::string short_authentication_string(const std::string_view local_fingerprint,
                                        const std::string_view peer_fingerprint,
                                        const std::string_view session_nonce) {
    const auto local = canonical(local_fingerprint);
    const auto peer = canonical(peer_fingerprint);
    if (local.empty() || peer.empty() || session_nonce.empty()) {
        throw std::invalid_argument("SAS inputs must not be empty");
    }
    const auto digest = sha256_hex(local + ":" + peer + ":" + std::string(session_nonce));
    std::string result = digest.substr(0, 8);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](const unsigned char value) { return static_cast<char>(std::toupper(value)); });
    return result;
}

} // namespace wabridge::pairing
