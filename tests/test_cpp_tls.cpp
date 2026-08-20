#include "test_require.h"
#include "wabridge_tls.h"

#include <cassert>
#include <iostream>

int main() {
    const auto context = wabridge::tls::Context::client_pairing();
    REQUIRE(context.native() != nullptr);
    REQUIRE(SSL_CTX_get_min_proto_version(context.native()) == TLS1_3_VERSION);
    REQUIRE(SSL_CTX_get_max_proto_version(context.native()) == TLS1_3_VERSION);
    REQUIRE(SSL_CTX_get_max_early_data(context.native()) == 0);
    REQUIRE(!wabridge::tls::negotiated_tls13(nullptr));
    std::cout << "TLS profile tests passed\n";
    return 0;
}
