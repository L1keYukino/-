#pragma once
#include <functional>
#include <string>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vim {

// System tray icon with context menu.
class TrayIcon {
public:
    using Callback = std::function<void()>;

    TrayIcon();
    ~TrayIcon();

    bool create(HINSTANCE hinst, HWND parent);
    void destroy();

    void set_recording(bool on);
    void set_menu_start_callback(Callback cb) { on_start_ = std::move(cb); }
    void set_menu_stop_callback(Callback cb)  { on_stop_ = std::move(cb); }
    void set_menu_quit_callback(Callback cb)  { on_quit_ = std::move(cb); }
    void set_left_click_callback(Callback cb) { on_click_ = std::move(cb); }

    void set_tooltip(const std::string& text);
    void show_menu();

private:
    void update_icon();

    NOTIFYICONDATAW nid_{};
    HWND parent_ = nullptr;
    HICON icon_idle_ = nullptr;
    HICON icon_rec_ = nullptr;
    std::atomic<bool> recording_{false};
    std::atomic<bool> created_{false};

    Callback on_start_;
    Callback on_stop_;
    Callback on_quit_;
    Callback on_click_;
};

} // namespace vim
