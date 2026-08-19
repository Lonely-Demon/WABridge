#include "wabridge_tls.h"

#include <cassert>
#include <iostream>

int main() {
    const auto context = wabridge::tls::Context::client_pairing();
    assert(context.native() != nullptr);
    assert(SSL_CTX_get_min_proto_version(context.native()) == TLS1_3_VERSION);
    assert(SSL_CTX_get_max_proto_version(context.native()) == TLS1_3_VERSION);
    assert(SSL_CTX_get_max_early_data(context.native()) == 0);
    assert(!wabridge::tls::negotiated_tls13(nullptr));
    std::cout << "TLS profile tests passed\n";
    return 0;
}
