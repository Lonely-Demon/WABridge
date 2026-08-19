#include "wabridge_discovery_cache.h"

#include <algorithm>
#include <utility>

namespace wabridge::discovery {

namespace {
constexpr std::size_t kMaxEntries = 64;
constexpr std::size_t kMaxAddressesPerEntry = 8;
constexpr auto kMaxTtl = std::chrono::hours(1);

std::chrono::seconds bounded_ttl(const std::chrono::seconds ttl) {
    if (ttl <= std::chrono::seconds::zero()) return std::chrono::seconds::zero();
    return std::min(ttl, std::chrono::duration_cast<std::chrono::seconds>(kMaxTtl));
}

RecordCache::Entry* find_entry(std::vector<RecordCache::Entry>& entries, std::string_view instance) {
    const auto it = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.instance == instance;
    });
    return it == entries.end() ? nullptr : &*it;
}

} // namespace

void RecordCache::apply_ptr(const std::string_view instance, const std::chrono::seconds ttl,
                            const Clock::time_point now) {
    if (instance.empty() || instance.size() > 253) return;
    auto* entry = find_entry(entries_, instance);
    if (entry == nullptr) {
        if (entries_.size() >= kMaxEntries) return;
        entries_.push_back(Entry{});
        entry = &entries_.back();
        entry->instance = instance;
    }
    entry->ptr_expires = now + bounded_ttl(ttl);
}

void RecordCache::apply_srv(const std::string_view instance, const std::string_view host,
                            const std::uint16_t port, const std::chrono::seconds ttl,
                            const Clock::time_point now) {
    if (instance.empty() || host.empty() || port == 0) return;
    auto* entry = find_entry(entries_, instance);
    if (entry == nullptr) return;
    entry->host = host;
    entry->port = port;
    entry->srv_expires = now + bounded_ttl(ttl);
}

void RecordCache::apply_address(const std::string_view host, const std::string_view address,
                                const std::chrono::seconds ttl, const Clock::time_point now) {
    if (host.empty() || address.empty()) return;
    const auto expiry = now + bounded_ttl(ttl);
    for (auto& entry : entries_) {
        if (entry.host != host) continue;
        if (std::find(entry.addresses.begin(), entry.addresses.end(), address) == entry.addresses.end()) {
            if (entry.addresses.size() >= kMaxAddressesPerEntry) continue;
            entry.addresses.push_back(std::string(address));
        }
        entry.address_expires = std::max(entry.address_expires, expiry);
    }
}

std::vector<ServiceEndpoint> RecordCache::ready(const Clock::time_point now) const {
    std::vector<ServiceEndpoint> result;
    for (const auto& entry : entries_) {
        if (entry.ptr_expires <= now || entry.srv_expires <= now || entry.address_expires <= now ||
            entry.host.empty() || entry.port == 0 || entry.addresses.empty()) {
            continue;
        }
        result.push_back(ServiceEndpoint{entry.instance, entry.host, entry.port, entry.addresses});
    }
    return result;
}

void RecordCache::expire(const Clock::time_point now) {
    for (auto& entry : entries_) {
        if (entry.address_expires <= now) entry.addresses.clear();
        if (entry.srv_expires <= now) {
            entry.host.clear();
            entry.port = 0;
        }
    }
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const auto& entry) {
        return entry.ptr_expires <= now;
    }), entries_.end());
}

void RecordCache::clear() noexcept {
    entries_.clear();
}

} // namespace wabridge::discovery
