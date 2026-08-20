#pragma once

#include "wabridge_audio.h"
#include "wabridge_clipboard.h"
#include "wabridge_display.h"
#include "wabridge_envelope.h"
#include "wabridge_file.h"
#include "wabridge_input.h"

#include <functional>

namespace wabridge::features {

constexpr std::uint16_t kFileOffer = 0x0001;
constexpr std::uint16_t kFileChunk = 0x0002;
constexpr std::uint16_t kClipboardUpdate = 0x0001;
constexpr std::uint16_t kAudioFrame = 0x0001;
constexpr std::uint16_t kInputEvent = 0x0102;
constexpr std::uint16_t kDisplayCommand = 0x0101;

class Dispatcher final {
public:
    std::function<bool(const file::Offer&)> on_file_offer;
    std::function<bool(const file::Chunk&)> on_file_chunk;
    std::function<bool(const clipboard::Update&)> on_clipboard_update;
    std::function<bool(const audio::Frame&)> on_audio_frame;
    std::function<bool(const input::Event&)> on_input_event;
    std::function<bool(const display::Command&)> on_display_command;

    bool dispatch(const protocol::Envelope& envelope) const noexcept;
};

} // namespace wabridge::features
