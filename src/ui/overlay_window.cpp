#include "src/ui/overlay_window.hpp"
#include <spdlog/spdlog.h>

namespace vim {

static const wchar_t* OVERLAY_CLASS = L"VIM_OverlayWindow";

static std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    w.resize(len - 1); // remove null terminator
    return w;
}

OverlayWindow::OverlayWindow() = default;

OverlayWindow::~OverlayWindow() {
    destroy();
}

bool OverlayWindow::create(HINSTANCE hinst) {
    hinst_ = hinst;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(bg_color_);
    wc.lpszClassName = OVERLAY_CLASS;
    RegisterClassExW(&wc);

    // Bottom-right corner of screen
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = sw - width_ - 20;
    int y = sh - height_ - 60;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        OVERLAY_CLASS, L"VIM Status",
        WS_POPUP,
        x, y, width_, height_,
        nullptr, nullptr, hinst, this);

    if (!hwnd_) return false;

    SetLayeredWindowAttributes(hwnd_, 0, 200, LWA_ALPHA); // 80% opacity

    // Create fonts
    hfont_ = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");

    hfont_big_ = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");

    spdlog::info("OverlayWindow created at {},{} ({}x{})", x, y, width_, height_);
    return true;
}

void OverlayWindow::destroy() {
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    if (hfont_) { DeleteObject(hfont_); hfont_ = nullptr; }
    if (hfont_big_) { DeleteObject(hfont_big_); hfont_big_ = nullptr; }
}

void OverlayWindow::show_recording() {
    std::lock_guard<std::mutex> lock(text_mutex_);
    status_line_ = L"🎤 正在录音...";
    asr_line_.clear();
    output_line_.clear();
    visible_.store(true);
    ShowWindow(hwnd_, SW_SHOW);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void OverlayWindow::show_processing(const std::string& text) {
    std::lock_guard<std::mutex> lock(text_mutex_);
    status_line_ = L"⏳ 处理中...";
    asr_line_ = utf8_to_wide(text);
    visible_.store(true);
    ShowWindow(hwnd_, SW_SHOW);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void OverlayWindow::show_result(const std::string& text) {
    std::lock_guard<std::mutex> lock(text_mutex_);
    status_line_ = L"✅ 已完成";
    output_line_ = utf8_to_wide(text);
    visible_.store(true);
    ShowWindow(hwnd_, SW_SHOW);
    InvalidateRect(hwnd_, nullptr, TRUE);

    // Auto-hide after 3 seconds
    std::thread([this]() {
        Sleep(3000);
        hide();
    }).detach();
}

void OverlayWindow::hide() {
    visible_.store(false);
    ShowWindow(hwnd_, SW_HIDE);
}

void OverlayWindow::set_asr_text(const std::string& text) {
    if (!visible_.load()) return;
    std::lock_guard<std::mutex> lock(text_mutex_);
    asr_line_ = utf8_to_wide(text);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void OverlayWindow::draw_text(HDC hdc, const std::wstring& text, int y, COLORREF color, int size) {
    if (text.empty()) return;
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, size == 22 ? hfont_big_ : hfont_);
    RECT r{15, y, width_ - 15, y + size + 8};
    DrawTextW(hdc, text.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_WORD_ELLIPSIS);
}

LRESULT CALLBACK OverlayWindow::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    case WM_PAINT: {
        if (!self) return 0;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Background
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(self->bg_color_);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        // Round rect border
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
        SelectObject(hdc, pen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rc.left + 3, rc.top + 3, rc.right - 3, rc.bottom - 3, 12, 12);
        DeleteObject(pen);

        std::lock_guard<std::mutex> lock(self->text_mutex_);
        self->draw_text(hdc, self->status_line_, 12, RGB(255, 255, 255), 22);
        if (!self->asr_line_.empty())
            self->draw_text(hdc, L"📝 " + self->asr_line_, 48, RGB(180, 180, 180), 16);
        if (!self->output_line_.empty())
            self->draw_text(hdc, L"📋 " + self->output_line_, 100, RGB(100, 200, 100), 16);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace vim
