#include "test_require.h"
#include "wabridge_discovery_cache.h"

#include <cassert>
#include <chrono>
#include <iostream>

using wabridge::discovery::RecordCache;

int main() {
    const auto start = RecordCache::Clock::now();
    RecordCache cache;
    cache.apply_ptr("PHONE._wabridge._tcp.local", std::chrono::seconds(30), start);
    REQUIRE(cache.ready(start).empty());

    cache.apply_srv("PHONE._wabridge._tcp.local", "phone.local", 51820,
                    std::chrono::seconds(30), start);
    REQUIRE(cache.ready(start).empty());

    cache.apply_address("phone.local", "192.168.1.20", std::chrono::seconds(30), start);
    const auto ready = cache.ready(start);
    REQUIRE(ready.size() == 1);
    REQUIRE(ready[0].instance == "PHONE._wabridge._tcp.local");
    REQUIRE(ready[0].host == "phone.local");
    REQUIRE(ready[0].port == 51820);
    REQUIRE(ready[0].addresses.size() == 1);

    cache.apply_address("phone.local", "192.168.1.21", std::chrono::seconds(30), start);
    REQUIRE(cache.ready(start)[0].addresses.size() == 2);

    cache.expire(start + std::chrono::seconds(31));
    REQUIRE(cache.ready(start + std::chrono::seconds(31)).empty());

    std::cout << "Discovery cache tests passed\n";
    return 0;
}
