#include "wabridge_clipboard.h"

#include "wabridge_envelope.h"

#include <algorithm>

namespace wabridge::clipboard {
namespace {
constexpr std::size_t kMaxDeviceId = 64;
constexpr std::size_t kMaxText = 1 * 1024 * 1024;

void put_u64(std::vector<std::uint8_t>& output, const std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) output.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& input, const std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) value = (value << 8) | input[offset + i];
    return value;
}

void require_ascii(std::string_view value, std::size_t max, const char* field) {
    if (value.empty() || value.size() > max) throw protocol::ProtocolError(std::string("invalid ") + field);
    for (const unsigned char character : value) {
        if (character < 0x20 || character > 0x7e) throw protocol::ProtocolError(std::string("invalid ") + field);
    }
}

} // namespace

std::vector<std::uint8_t> encode(const Update& update) {
    require_ascii(update.origin_device_id, kMaxDeviceId, "origin device id");
    if (update.text.empty() || update.text.size() > kMaxText) throw protocol::ProtocolError("invalid clipboard text");
    if (update.timestamp_ms == 0) throw protocol::ProtocolError("invalid clipboard timestamp");

    std::vector<std::uint8_t> output;
    output.reserve(16 + 8 + 1 + update.origin_device_id.size() + 4 + update.text.size());
    output.insert(output.end(), update.loop_token.begin(), update.loop_token.end());
    put_u64(output, update.timestamp_ms);
    output.push_back(static_cast<std::uint8_t>(update.origin_device_id.size()));
    output.insert(output.end(), update.origin_device_id.begin(), update.origin_device_id.end());
    const auto text_size = static_cast<std::uint32_t>(update.text.size());
    output.push_back(static_cast<std::uint8_t>(text_size >> 24));
    output.push_back(static_cast<std::uint8_t>(text_size >> 16));
    output.push_back(static_cast<std::uint8_t>(text_size >> 8));
    output.push_back(static_cast<std::uint8_t>(text_size));
    output.insert(output.end(), update.text.begin(), update.text.end());
    return output;
}

Update decode(const std::vector<std::uint8_t>& payload) {
    if (payload.size() < 16 + 8 + 1 + 4 + 1) throw protocol::ProtocolError("truncated clipboard update");
    Update update;
    std::size_t offset = 0;
    std::copy_n(payload.begin(), update.loop_token.size(), update.loop_token.begin());
    offset += update.loop_token.size();
    update.timestamp_ms = read_u64(payload, offset);
    offset += 8;
    const auto device_length = payload[offset++];
    if (device_length == 0 || device_length > kMaxDeviceId || offset + device_length + 4 > payload.size()) {
        throw protocol::ProtocolError("invalid clipboard device id length");
    }
    update.origin_device_id.assign(reinterpret_cast<const char*>(payload.data() + offset), device_length);
    offset += device_length;
    const auto text_length = (static_cast<std::uint32_t>(payload[offset]) << 24) |
                             (static_cast<std::uint32_t>(payload[offset + 1]) << 16) |
                             (static_cast<std::uint32_t>(payload[offset + 2]) << 8) |
                             static_cast<std::uint32_t>(payload[offset + 3]);
    offset += 4;
    if (text_length == 0 || text_length > kMaxText || payload.size() != offset + text_length) {
        throw protocol::ProtocolError("invalid clipboard text length");
    }
    update.text.assign(reinterpret_cast<const char*>(payload.data() + offset), text_length);
    require_ascii(update.origin_device_id, kMaxDeviceId, "origin device id");
    if (update.timestamp_ms == 0) throw protocol::ProtocolError("invalid clipboard timestamp");
    return update;
}

bool LoopGuard::should_apply(const std::array<std::uint8_t, 16>& token) {
    if (has_token_ && token == last_token_) return false;
    last_token_ = token;
    has_token_ = true;
    return true;
}

void LoopGuard::mark_local(const std::array<std::uint8_t, 16>& token) {
    last_token_ = token;
    has_token_ = true;
}

} // namespace wabridge::clipboard
