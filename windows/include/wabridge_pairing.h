#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wabridge::pairing {

enum class State {
    Unknown,
    AwaitingUserApproval,
    Authenticated,
    KnownOffline,
    BlockedChangedIdentity,
};

struct Record {
    std::string device_id;
    std::string display_name;
    std::string certificate_fingerprint;
};

class Store final {
public:
    void remember(Record record);
    void revoke(std::string_view device_id);
    std::optional<Record> find(std::string_view device_id) const;
    State evaluate(std::string_view device_id, std::string_view peer_fingerprint) const;

private:
    std::optional<Record> record_;
};

// Returns eight uppercase hexadecimal characters. It is a human comparison
// aid, never a password or key. Fingerprints must be canonical SHA-256 text.
std::string short_authentication_string(std::string_view local_fingerprint,
                                        std::string_view peer_fingerprint,
                                        std::string_view session_nonce);

} // namespace wabridge::pairing
