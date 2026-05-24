#pragma once
#include <atomic>
#include <string>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#include <gdiplus.h>
#endif

namespace vim {

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    bool create(HINSTANCE hinst);
    void destroy();
    void show_recording();
    void show_processing(const std::string& text);
    void show_result(const std::string& text);
    void hide();
    void set_audio_level(float db);

private:
    static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);

    HWND hwnd_ = nullptr;
    int state_ = 0;
    float db_ = -60.0f;
    float dot_y_[9]{};
    float dot_sens_[9];
    HINSTANCE hinst_ = nullptr;
    ULONG_PTR gdiplus_token_ = 0;
    int hide_generation_ = 0; // cancel stale hide threads
};

} // namespace vim
