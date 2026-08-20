#include "wabridge_feature_dispatch.h"

namespace wabridge::features {

bool Dispatcher::dispatch(const protocol::Envelope& envelope) const noexcept {
    try {
        switch (envelope.channel) {
        case 1:
            if (envelope.kind == kDisplayCommand && on_display_command) {
                return on_display_command(display::decode(envelope.payload));
            }
            if (envelope.kind == kInputEvent && on_input_event) {
                return on_input_event(input::decode_event(envelope.payload));
            }
            return false;
        case 2:
            return false;
        case 3:
            if (envelope.kind == kFileOffer && on_file_offer) {
                return on_file_offer(file::decode_offer(envelope.payload));
            }
            if (envelope.kind == kFileChunk && on_file_chunk) {
                return on_file_chunk(file::decode_chunk(envelope.payload));
            }
            return false;
        case 4:
            if (envelope.kind != kClipboardUpdate || !on_clipboard_update) return false;
            return on_clipboard_update(clipboard::decode(envelope.payload));
        case 5:
            if (envelope.kind != kAudioFrame || !on_audio_frame) return false;
            return on_audio_frame(audio::decode_frame(envelope.payload));
        default:
            return false;
        }
    } catch (const protocol::ProtocolError&) {
        return false;
    }
}

} // namespace wabridge::features
