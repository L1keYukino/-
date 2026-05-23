#pragma once
#include <atomic>
#include <string>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vim {

// Semi-transparent floating window showing recording/processing status.
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
    void set_asr_text(const std::string& text);

private:
    static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
    void update_content();
    void draw_text(HDC hdc, const std::wstring& text, int y, COLORREF color, int size);

    HWND hwnd_ = nullptr;
    std::wstring status_line_;
    std::wstring asr_line_;
    std::wstring output_line_;
    float audio_level_db_ = -60.0f;
    std::mutex text_mutex_;
    std::atomic<bool> visible_{false};
    HINSTANCE hinst_ = nullptr;
    int width_ = 420;
    int height_ = 180;
    HFONT hfont_ = nullptr;
    HFONT hfont_big_ = nullptr;
    COLORREF bg_color_ = RGB(30, 30, 30);
};

} // namespace vim
