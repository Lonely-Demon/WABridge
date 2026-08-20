#include "wabridge_wasapi_audio.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <windows.h>

namespace {
template <typename T>
void release_com(T*& pointer) noexcept {
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}
}
#endif

namespace wabridge::platform_audio {

WasapiRenderer::~WasapiRenderer() {
    stop();
}

bool WasapiRenderer::start(const std::uint32_t sample_rate, const std::uint8_t channels) {
    if (sample_rate < 8'000 || sample_rate > 192'000 || channels < 1 || channels > 2) return false;
    stop();
#ifdef _WIN32
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return false;
    com_initialized_ = true;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = sample_rate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(channels * sizeof(std::int16_t));
    format.nAvgBytesPerSec = sample_rate * format.nBlockAlign;
    format.cbSize = 0;
    bool ok = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                         IID_PPV_ARGS(&enumerator))) &&
              SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
              SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                         reinterpret_cast<void**>(&client)));
    if (ok) {
        ok = SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10'000'000, 0, &format, nullptr));
    }
    UINT32 buffer_frames = 0;
    IAudioRenderClient* render = nullptr;
    if (ok) ok = SUCCEEDED(client->GetBufferSize(&buffer_frames));
    if (ok) ok = SUCCEEDED(client->GetService(IID_PPV_ARGS(&render)));
    if (ok) ok = SUCCEEDED(client->Start());
    if (!ok) {
        release_com(client);
        release_com(device);
        release_com(enumerator);
        CoUninitialize();
        com_initialized_ = false;
        return false;
    }
    audio_client_ = client;
    render_client_ = render;
    render = nullptr;
    if (!render_client_) {
        release_com(client);
        release_com(device);
        release_com(enumerator);
        CoUninitialize();
        com_initialized_ = false;
        return false;
    }
    buffer_frames_ = buffer_frames;
    sample_rate_ = sample_rate;
    channels_ = channels;
    release_com(device);
    release_com(enumerator);
    running_ = true;
    return true;
#else
    (void)sample_rate;
    (void)channels;
    return false;
#endif
}

bool WasapiRenderer::render(const audio::Frame& frame) {
    if (!running_ || frame.codec != audio::Codec::Pcm16 || frame.sample_rate != sample_rate_ ||
        frame.channels != channels_ || frame.data.empty()) return false;
#ifdef _WIN32
    auto* client = static_cast<IAudioClient*>(audio_client_);
    auto* render = static_cast<IAudioRenderClient*>(render_client_);
    UINT32 padding = 0;
    if (FAILED(client->GetCurrentPadding(&padding)) || padding >= buffer_frames_) return false;
    const auto frame_bytes = static_cast<std::size_t>(channels_) * sizeof(std::int16_t);
    const auto requested = static_cast<UINT32>(frame.data.size() / frame_bytes);
    const auto frames = (std::min)(requested, buffer_frames_ - padding);
    if (frames == 0) return false;
    BYTE* destination = nullptr;
    if (FAILED(render->GetBuffer(frames, &destination))) return false;
    std::memcpy(destination, frame.data.data(), static_cast<std::size_t>(frames) * frame_bytes);
    return SUCCEEDED(render->ReleaseBuffer(frames, 0));
#else
    return false;
#endif
}

void WasapiRenderer::stop() noexcept {
#ifdef _WIN32
    auto* client = static_cast<IAudioClient*>(audio_client_);
    if (client) client->Stop();
    if (render_client_) {
        static_cast<IAudioRenderClient*>(render_client_)->Release();
        render_client_ = nullptr;
    }
    if (audio_client_) {
        static_cast<IAudioClient*>(audio_client_)->Release();
        audio_client_ = nullptr;
    }
    buffer_frames_ = 0;
    sample_rate_ = 0;
    channels_ = 0;
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
#endif
    running_ = false;
}

} // namespace wabridge::platform_audio
