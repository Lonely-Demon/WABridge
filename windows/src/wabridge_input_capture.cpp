#include "wabridge_input_capture.h"

#include <utility>

#ifdef _WIN32
#include <windows.h>

namespace {

wabridge::platform_input::LowLevelCapture* g_capture = nullptr;

LRESULT CALLBACK mouse_proc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && g_capture) {
        const auto* event = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        if ((event->flags & LLMHF_INJECTED) == 0) {
            wabridge::input::Event output;
            output.x = event->pt.x;
            output.y = event->pt.y;
            switch (message) {
            case WM_MOUSEMOVE:
                output.type = wabridge::input::Type::MouseMove;
                break;
            case WM_LBUTTONDOWN: output.type = wabridge::input::Type::MouseButton; output.button = 1; output.flags = 1; break;
            case WM_LBUTTONUP: output.type = wabridge::input::Type::MouseButton; output.button = 1; break;
            case WM_RBUTTONDOWN: output.type = wabridge::input::Type::MouseButton; output.button = 2; output.flags = 1; break;
            case WM_RBUTTONUP: output.type = wabridge::input::Type::MouseButton; output.button = 2; break;
            case WM_MBUTTONDOWN: output.type = wabridge::input::Type::MouseButton; output.button = 3; output.flags = 1; break;
            case WM_MBUTTONUP: output.type = wabridge::input::Type::MouseButton; output.button = 3; break;
            case WM_MOUSEWHEEL:
                output.type = wabridge::input::Type::MouseWheel;
                output.wheel = GET_WHEEL_DELTA_WPARAM(event->mouseData);
                break;
            default:
                return CallNextHookEx(nullptr, code, message, data);
            }
            g_capture->emit(output);
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

LRESULT CALLBACK keyboard_proc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && g_capture) {
        const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        if ((event->flags & LLKHF_INJECTED) == 0 &&
            (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP)) {
            wabridge::input::Event output;
            output.type = wabridge::input::Type::Key;
            output.code = event->vkCode;
            output.flags = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) ? 1 : 0;
            output.modifiers = static_cast<std::uint16_t>(GetKeyState(VK_SHIFT) & 0x8000 ? 1 : 0) |
                               static_cast<std::uint16_t>(GetKeyState(VK_CONTROL) & 0x8000 ? 2 : 0) |
                               static_cast<std::uint16_t>(GetKeyState(VK_MENU) & 0x8000 ? 4 : 0);
            g_capture->emit(output);
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

DWORD WINAPI hook_thread(LPVOID parameter) {
    auto* capture = static_cast<wabridge::platform_input::LowLevelCapture*>(parameter);
    g_capture = capture;
    HINSTANCE module = GetModuleHandleW(nullptr);
    auto* mouse = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, module, 0);
    auto* keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, module, 0);
    capture->set_hooks(mouse, keyboard, GetCurrentThreadId());
    capture->signal_ready();
    if (!mouse || !keyboard) {
        if (mouse) UnhookWindowsHookEx(mouse);
        if (keyboard) UnhookWindowsHookEx(keyboard);
        capture->set_hooks(nullptr, nullptr, GetCurrentThreadId());
        capture->mark_inactive();
        g_capture = nullptr;
        return 1;
    }
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0 && capture->active()) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    UnhookWindowsHookEx(mouse);
    UnhookWindowsHookEx(keyboard);
    capture->set_hooks(nullptr, nullptr, GetCurrentThreadId());
    g_capture = nullptr;
    return 0;
}

} // namespace
#endif

namespace wabridge::platform_input {

LowLevelCapture::LowLevelCapture(Callback callback) : callback_(std::move(callback)) {}

LowLevelCapture::~LowLevelCapture() {
    stop();
}

bool LowLevelCapture::start() {
    if (active_ || !callback_) return false;
#ifdef _WIN32
    ready_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ready_event_) return false;
    active_ = true;
    thread_ = CreateThread(nullptr, 0, hook_thread, this, 0, nullptr);
    if (!thread_) {
        CloseHandle(static_cast<HANDLE>(ready_event_));
        ready_event_ = nullptr;
        active_ = false;
        return false;
    }
    const auto ready = WaitForSingleObject(static_cast<HANDLE>(ready_event_), 2000);
    if (ready != WAIT_OBJECT_0 || !active_) {
        stop();
        return false;
    }
    return true;
#else
    return false;
#endif
}

void LowLevelCapture::stop() noexcept {
#ifdef _WIN32
    if (thread_) {
        active_ = false;
        PostThreadMessageW(GetThreadId(static_cast<HANDLE>(thread_)), WM_QUIT, 0, 0);
        WaitForSingleObject(static_cast<HANDLE>(thread_), 2000);
        CloseHandle(static_cast<HANDLE>(thread_));
        thread_ = nullptr;
    }
    if (ready_event_) {
        CloseHandle(static_cast<HANDLE>(ready_event_));
        ready_event_ = nullptr;
    }
#endif
    active_ = false;
}

void LowLevelCapture::emit(const input::Event& event) {
    if (active_ && callback_) callback_(event);
}

#ifdef _WIN32
void LowLevelCapture::set_hooks(void* mouse, void* keyboard, unsigned long) {
    mouse_hook_ = mouse;
    keyboard_hook_ = keyboard;
}

void LowLevelCapture::signal_ready() {
    if (ready_event_) SetEvent(static_cast<HANDLE>(ready_event_));
}

void LowLevelCapture::mark_inactive() noexcept {
    active_ = false;
}
#endif

} // namespace wabridge::platform_input
