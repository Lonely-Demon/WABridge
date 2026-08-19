#include "wabridge_pairing.h"

#include <cassert>
#include <iostream>

using wabridge::pairing::Record;
using wabridge::pairing::State;
using wabridge::pairing::Store;
using wabridge::pairing::short_authentication_string;

int main() {
    Store store;
    assert(store.evaluate("android-1", "AA:BB") == State::Unknown);

    store.remember(Record{"android-1", "Test phone", "AA:BB:CC"});
    assert(store.evaluate("android-1", "aa bb cc") == State::Authenticated);
    assert(store.evaluate("android-1", "AA:BB:DD") == State::BlockedChangedIdentity);
    assert(store.evaluate("other", "AA:BB:CC") == State::Unknown);

    const auto sas_a = short_authentication_string(
        "AA:BB:CC", "11:22:33", "00112233445566778899AABBCCDDEEFF");
    const auto sas_b = short_authentication_string(
        "aabbcc", "112233", "00112233445566778899AABBCCDDEEFF");
    assert(sas_a == sas_b);
    assert(sas_a.size() == 8);

    store.revoke("android-1");
    assert(store.evaluate("android-1", "AA:BB:CC") == State::Unknown);

    std::cout << "Pairing tests passed: SAS=" << sas_a << "\n";
    return 0;
}
