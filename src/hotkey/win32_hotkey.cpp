#include "src/hotkey/win32_hotkey.hpp"
#include <cstdio>
#include <spdlog/spdlog.h>

#ifdef _WIN32
  #include <windows.h>
  #define VIM_HAS_WIN32_HOTKEY 1
#else
  #define VIM_HAS_WIN32_HOTKEY 0
#endif

namespace vim {

#if VIM_HAS_WIN32_HOTKEY

static const wchar_t* WINDOW_CLASS_NAME = L"VoiceInputMethod_HotkeyWindow";

struct Win32HotkeyManager::Impl {
    HWND hwnd = nullptr;
    HINSTANCE hinstance = nullptr;
    HotkeyCallback callback;
    UINT hotkey_mods = 0;
    UINT hotkey_vk = 0;
    bool hotkey_registered = false;
};

static LRESULT CALLBACK hotkey_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_HOTKEY) {
        auto* self = reinterpret_cast<Win32HotkeyManager*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        // Debounce: ignore rapid re-triggers within 500ms
        static DWORD last_time = 0;
        DWORD now = GetTickCount();
        if (now - last_time < 500) return 0;
        last_time = now;

        if (self) {
            auto* cb = self->get_callback();
            if (cb && *cb) {
                self->toggle_ptt();
            }
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

#else

struct Win32HotkeyManager::Impl {
    HotkeyCallback callback;
    unsigned hotkey_mods = 0;
    unsigned hotkey_vk = 0;
    bool hotkey_registered = false;
};

#endif

Win32HotkeyManager::Win32HotkeyManager()
    : impl_(std::make_unique<Impl>())
{
}

HotkeyCallback* Win32HotkeyManager::get_callback() { return &impl_->callback; }
unsigned Win32HotkeyManager::get_hotkey_vk() const { return impl_->hotkey_vk; }

void Win32HotkeyManager::toggle_ptt() {
    ptt_active_ = !ptt_active_;
    if (impl_->callback) {
        impl_->callback(ptt_active_);
    }
}

Win32HotkeyManager::~Win32HotkeyManager() {
    unregister_hotkey();
}

bool Win32HotkeyManager::register_hotkey(HotkeyMod modifiers, uint32_t virtual_key,
                                          HotkeyCallback callback) {
#if VIM_HAS_WIN32_HOTKEY
    if (impl_->hotkey_registered) unregister_hotkey();

    impl_->callback = std::move(callback);
    impl_->hinstance = GetModuleHandleW(nullptr);

    // Create message-only window
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = hotkey_wnd_proc;
    wc.hInstance = impl_->hinstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassExW(&wc);

    impl_->hwnd = CreateWindowExW(0, WINDOW_CLASS_NAME, L"",
                                   WS_OVERLAPPED, 0, 0, 0, 0,
                                   HWND_MESSAGE, nullptr, impl_->hinstance, nullptr);
    if (!impl_->hwnd) {
        spdlog::error("Win32Hotkey: CreateWindowEx failed");
        return false;
    }

    // Store this pointer for the window proc
    SetWindowLongPtrW(impl_->hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));

    // Convert our HotkeyMod to Win32 MOD_* flags
    UINT mods = 0;
    uint32_t m = static_cast<uint32_t>(modifiers);
    if (m & 0x0001) mods |= MOD_ALT;
    if (m & 0x0002) mods |= MOD_CONTROL;
    if (m & 0x0004) mods |= MOD_SHIFT;
    if (m & 0x0008) mods |= MOD_WIN;

    if (!RegisterHotKey(impl_->hwnd, hotkey_id_, mods, virtual_key)) {
        spdlog::error("Win32Hotkey: RegisterHotKey failed (mods={}, vk=0x{:X})",
                      mods, virtual_key);
        DestroyWindow(impl_->hwnd);
        impl_->hwnd = nullptr;
        return false;
    }

    impl_->hotkey_mods = mods;
    impl_->hotkey_vk = virtual_key;
    impl_->hotkey_registered = true;

    spdlog::info("Win32Hotkey: registered hotkey Ctrl+Alt+{:c}", virtual_key);
    return true;
#else
    (void)modifiers; (void)virtual_key;
    impl_->callback = std::move(callback);
    spdlog::warn("Win32Hotkey: not available (non-Windows)");
    return false;
#endif
}

void Win32HotkeyManager::unregister_hotkey() {
#if VIM_HAS_WIN32_HOTKEY
    if (impl_->hotkey_registered) {
        UnregisterHotKey(impl_->hwnd, hotkey_id_);
        impl_->hotkey_registered = false;
    }
    if (impl_->hwnd) {
        DestroyWindow(impl_->hwnd);
        impl_->hwnd = nullptr;
    }
    if (impl_->hinstance) {
        UnregisterClassW(WINDOW_CLASS_NAME, impl_->hinstance);
    }
#endif
}

bool Win32HotkeyManager::pump_messages() {
#if VIM_HAS_WIN32_HOTKEY
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
#else
    return false;
#endif
}

void Win32HotkeyManager::run_message_loop() {
#if VIM_HAS_WIN32_HOTKEY
    spdlog::info("Win32Hotkey: entering message loop");
    running_.store(true);

    MSG msg;
    while (running_.load() && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    spdlog::info("Win32Hotkey: message loop exited");
#endif
}

void Win32HotkeyManager::quit() {
#if VIM_HAS_WIN32_HOTKEY
    running_.store(false);
    PostQuitMessage(0);
#endif
}

} // namespace vim
