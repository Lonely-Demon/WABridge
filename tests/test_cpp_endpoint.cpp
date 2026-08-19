#include "wabridge_endpoint.h"

#include <cassert>
#include <iostream>

using wabridge::discovery::parse_endpoint;

int main() {
    auto endpoint = parse_endpoint("  192.168.1.10:51820 ");
    assert(endpoint.has_value());
    assert(endpoint->host == "192.168.1.10");
    assert(endpoint->port == 51820);

    endpoint = parse_endpoint("laptop.local", 40000);
    assert(endpoint.has_value() && endpoint->host == "laptop.local" && endpoint->port == 40000);

    endpoint = parse_endpoint("[2001:db8::10]:1234");
    assert(endpoint.has_value() && endpoint->host == "2001:db8::10" && endpoint->port == 1234);

    endpoint = parse_endpoint("2001:db8::10", 5000);
    assert(endpoint.has_value() && endpoint->host == "2001:db8::10" && endpoint->port == 5000);

    assert(!parse_endpoint("[2001:db8::10]bad").has_value());
    assert(!parse_endpoint("192.168.1.10:0").has_value());
    assert(!parse_endpoint("192.168.1.10:65536").has_value());
    assert(!parse_endpoint("192.168.1.10:abc").has_value());
    assert(!parse_endpoint("2001:db8::10:1234").has_value());
    assert(!parse_endpoint("laptop local", 51820).has_value());

    std::cout << "Endpoint parser tests passed\n";
    return 0;
}
