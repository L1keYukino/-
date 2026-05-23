#include "src/ui/tray_icon.hpp"
#include <spdlog/spdlog.h>

namespace vim {

// Create a simple colored circle icon
static HICON create_circle_icon(HINSTANCE hinst, COLORREF color, int size) {
    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp) { DeleteDC(memDC); ReleaseDC(nullptr, hdc); return nullptr; }

    SelectObject(memDC, hBmp);
    HBRUSH brush = CreateSolidBrush(color);
    RECT r{0, 0, size, size};
    FillRect(memDC, &r, brush);
    DeleteObject(brush);

    // White "V" or dot in center
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    SelectObject(memDC, pen);
    int cx = size / 2;
    Ellipse(memDC, cx - 4, cx - 4, cx + 4, cx + 4);
    DeleteObject(pen);

    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = hBmp;
    ii.hbmMask = hBmp;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(hBmp);
    return icon;
}

static constexpr UINT WM_TRAYICON = WM_APP + 1;

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    destroy();
}

bool TrayIcon::create(HINSTANCE hinst, HWND parent) {
    parent_ = parent;

    icon_idle_ = create_circle_icon(hinst, RGB(80, 80, 80), 32);
    icon_rec_  = create_circle_icon(hinst, RGB(220, 50, 50), 32);

    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = parent;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = icon_idle_;
    wcscpy_s(nid_.szTip, L"语音输入法 - 空闲");

    if (!Shell_NotifyIconW(NIM_ADD, &nid_)) {
        spdlog::error("Failed to create tray icon");
        return false;
    }

    created_.store(true);
    spdlog::info("Tray icon created");
    return true;
}

void TrayIcon::destroy() {
    if (created_.exchange(false)) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
    }
    if (icon_idle_) { DestroyIcon(icon_idle_); icon_idle_ = nullptr; }
    if (icon_rec_)  { DestroyIcon(icon_rec_); icon_rec_ = nullptr; }
}

void TrayIcon::set_recording(bool on) {
    if (recording_.exchange(on) == on) return; // no change
    update_icon();
}

void TrayIcon::update_icon() {
    if (!created_.load()) return;
    nid_.hIcon = recording_.load() ? icon_rec_ : icon_idle_;
    wcscpy_s(nid_.szTip, recording_.load() ? L"语音输入法 - 录音中" : L"语音输入法 - 空闲");
    nid_.uFlags = NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::set_tooltip(const std::string& text) {
    if (!created_.load()) return;
    std::wstring w(text.begin(), text.end());
    wcsncpy_s(nid_.szTip, w.c_str(), _TRUNCATE);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::show_menu() {
    HMENU menu = CreatePopupMenu();

    if (recording_.load()) {
        AppendMenuW(menu, MF_STRING, 1, L"⏹ 停止录音");
    } else {
        AppendMenuW(menu, MF_STRING, 1, L"🎤 开始录音");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"❌ 退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(parent_);

    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                             pt.x, pt.y, 0, parent_, nullptr);
    DestroyMenu(menu);

    switch (cmd) {
    case 1:
        if (recording_.load() && on_stop_) on_stop_();
        else if (on_start_) on_start_();
        break;
    case 3:
        if (on_quit_) on_quit_();
        break;
    }
}

} // namespace vim
