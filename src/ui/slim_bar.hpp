#pragma once
#include <string>
#include <atomic>
#include <functional>
#ifdef _WIN32
#include <windows.h>
#endif

namespace vim {

using SlimBarCallback = std::function<void()>;

class SlimBar {
public:
    SlimBar();
    ~SlimBar();

    bool create(HINSTANCE hinst);
    void destroy();

    void set_state(int state);  // 0=idle, 1=rec, 2=proc, 3=result
    void set_audio_level(float db);
    void set_result_text(const std::string& utf8);

    void set_on_quit(SlimBarCallback cb) { on_quit_ = std::move(cb); }

private:
    static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void draw_vu_bars(Gdiplus::Graphics& g);

    HWND hwnd_ = nullptr;
    int w_ = 320, h_ = 36;

    int state_ = 0; // 0=idle,1=rec,2=proc,3=result
    float audio_db_ = -60.0f;
    std::wstring result_text_;

    SlimBarCallback on_quit_;
};

} // namespace vim
