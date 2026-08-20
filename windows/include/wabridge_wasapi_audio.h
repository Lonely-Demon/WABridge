#pragma once

#include "wabridge_audio.h"

#include <cstdint>
#include <vector>

namespace wabridge::platform_audio {

class WasapiRenderer final {
public:
    WasapiRenderer() = default;
    ~WasapiRenderer();
    WasapiRenderer(const WasapiRenderer&) = delete;
    WasapiRenderer& operator=(const WasapiRenderer&) = delete;

    bool start(std::uint32_t sample_rate, std::uint8_t channels);
    bool render(const audio::Frame& frame);
    void stop() noexcept;
    bool running() const noexcept { return running_; }

private:
    bool running_{false};
    std::uint32_t sample_rate_{0};
    std::uint8_t channels_{0};
#ifdef _WIN32
    void* audio_client_{nullptr};
    void* render_client_{nullptr};
    std::uint32_t buffer_frames_{0};
    bool com_initialized_{false};
#endif
};

} // namespace wabridge::platform_audio
