#pragma once

#include "wabridge_input.h"

#include <functional>

namespace wabridge::platform_input {

class LowLevelCapture final {
public:
    using Callback = std::function<void(const input::Event&)>;

    explicit LowLevelCapture(Callback callback);
    ~LowLevelCapture();
    LowLevelCapture(const LowLevelCapture&) = delete;
    LowLevelCapture& operator=(const LowLevelCapture&) = delete;

    bool start();
    void stop() noexcept;
    bool active() const noexcept { return active_; }
    void emit(const input::Event& event);
#ifdef _WIN32
    void mark_inactive() noexcept;
#endif
#ifdef _WIN32
    void set_hooks(void* mouse, void* keyboard, unsigned long thread_id);
    void signal_ready();
#endif

private:
    Callback callback_;
    bool active_{false};
#ifdef _WIN32
    void* mouse_hook_{nullptr};
    void* keyboard_hook_{nullptr};
    void* thread_{nullptr};
    void* ready_event_{nullptr};
#endif
};

} // namespace wabridge::platform_input
