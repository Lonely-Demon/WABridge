#include "test_require.h"
#include "wabridge_feature_dispatch.h"

#include <iostream>

int main() {
    wabridge::features::Dispatcher dispatcher;
    bool file_seen = false;
    bool clipboard_seen = false;
    bool audio_seen = false;
    bool input_seen = false;
    bool display_seen = false;

    dispatcher.on_file_offer = [&](const wabridge::file::Offer& offer) {
        file_seen = offer.display_name == "note.txt";
        return file_seen;
    };
    dispatcher.on_clipboard_update = [&](const wabridge::clipboard::Update& update) {
        clipboard_seen = update.text == "hello";
        return clipboard_seen;
    };
    dispatcher.on_audio_frame = [&](const wabridge::audio::Frame& frame) {
        audio_seen = frame.data.size() == 4;
        return audio_seen;
    };
    dispatcher.on_input_event = [&](const wabridge::input::Event& event) {
        input_seen = event.type == wabridge::input::Type::Key;
        return input_seen;
    };
    dispatcher.on_display_command = [&](const wabridge::display::Command& command) {
        display_seen = command.mode == wabridge::display::Mode::PhoneControl;
        return display_seen;
    };

    wabridge::file::Offer offer;
    offer.transfer_id.fill(1);
    offer.display_name = "note.txt";
    offer.mime_type = "text/plain";
    offer.size = 4;
    const auto offer_payload = wabridge::file::encode_offer(offer);
    REQUIRE(dispatcher.dispatch({3, wabridge::features::kFileOffer, 0, 1, offer_payload}));
    REQUIRE(file_seen);

    wabridge::clipboard::Update clipboard;
    clipboard.loop_token.fill(2);
    clipboard.origin_device_id = "android";
    clipboard.timestamp_ms = 10;
    clipboard.text = "hello";
    REQUIRE(dispatcher.dispatch({4, wabridge::features::kClipboardUpdate, 0, 2, wabridge::clipboard::encode(clipboard)}));
    REQUIRE(clipboard_seen);

    wabridge::audio::Frame audio;
    audio.codec = wabridge::audio::Codec::Opus;
    audio.channels = 1;
    audio.sample_rate = 48'000;
    audio.timestamp_ms = 10;
    audio.data.assign(4, 7);
    REQUIRE(dispatcher.dispatch({5, wabridge::features::kAudioFrame, 0, 3, wabridge::audio::encode_frame(audio)}));
    REQUIRE(audio_seen);

    wabridge::input::Event input;
    input.type = wabridge::input::Type::Key;
    input.flags = 1;
    input.code = 0x41;
    REQUIRE(dispatcher.dispatch({1, wabridge::features::kInputEvent, 0, 4, wabridge::input::encode_event(input)}));
    REQUIRE(input_seen);

    wabridge::display::Command display{wabridge::display::Mode::PhoneControl, false, 1};
    REQUIRE(dispatcher.dispatch({1, wabridge::features::kDisplayCommand, 0, 5, wabridge::display::encode(display)}));
    REQUIRE(display_seen);

    REQUIRE(!dispatcher.dispatch({3, wabridge::features::kFileOffer, 0, 6, {1, 2, 3}}));
    REQUIRE(!dispatcher.dispatch({2, 1, 0, 7, {1}}));

    std::cout << "Typed feature-dispatch tests passed\n";
    return 0;
}
