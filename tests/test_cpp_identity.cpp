#include "test_require.h"
#include "wabridge_identity.h"

#include <chrono>
#include <filesystem>
#include <iostream>

int main() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() /
                           ("wabridge-identity-test-" + std::to_string(unique));
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);

    wabridge::identity::Store store(directory);
    const auto first = store.load_or_create();
    REQUIRE(!first.certificate_pem.empty());
    REQUIRE(!first.private_key_pem.empty());
    REQUIRE(!first.fingerprint.empty());

    const auto second = store.load_or_create();
    REQUIRE(second.certificate_pem == first.certificate_pem);
    REQUIRE(second.private_key_pem == first.private_key_pem);
    REQUIRE(second.fingerprint == first.fingerprint);

    std::filesystem::remove_all(directory, cleanup_error);
    std::cout << "Identity-store tests passed\n";
    return 0;
}
