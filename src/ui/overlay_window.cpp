#include "src/ui/overlay_window.hpp"
#include <algorithm>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace vim {

using namespace Gdiplus;

static const wchar_t* CLASS_NAME = L"VIM_OverlayV3";
static const int N_DOTS   = 9;
static const int DOT_R    = 4;
static const int GAP      = 4;
static const int BAR_MAX  = 80;  // taller
static const int PAD      = 4;
static const int WIN_W    = N_DOTS * (DOT_R*2 + GAP) - GAP + PAD*2;
static const int WIN_H    = BAR_MAX + DOT_R*2 + PAD*2;

OverlayWindow::OverlayWindow() {
    for (int i = 0; i < N_DOTS; ++i) {
        dot_sens_[i] = 1.0f;
        dot_y_[i] = (float)(DOT_R * 2);
    }
}
OverlayWindow::~OverlayWindow() { destroy(); }

bool OverlayWindow::create(HINSTANCE hinst) {
    hinst_ = hinst;

    static bool gdi_ok = false;
    if (!gdi_ok) {
        GdiplusStartupInput gdi_in;
        gdi_ok = (GdiplusStartup(&gdiplus_token_, &gdi_in, nullptr) == Ok);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc); // ignore error (may already exist)

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME, L"", WS_POPUP,
        sw - WIN_W - 30, sh - WIN_H - 80, WIN_W, WIN_H,
        nullptr, nullptr, hinst, this);

    if (!hwnd_) {
        spdlog::error("Overlay: CreateWindowExW failed");
        return false;
    }
    SetLayeredWindowAttributes(hwnd_, RGB(0,0,0), 0, LWA_COLORKEY);
    spdlog::info("Overlay: window created {}x{}", WIN_W, WIN_H);
    return true;
}

void OverlayWindow::destroy() {
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    if (gdiplus_token_) { GdiplusShutdown(gdiplus_token_); gdiplus_token_ = 0; }
}

void OverlayWindow::show_idle() {
    state_ = 0; ShowWindow(hwnd_, SW_SHOW); InvalidateRect(hwnd_, nullptr, FALSE);
}
void OverlayWindow::show_recording() {
    hide_generation_++;
    state_ = 1; ShowWindow(hwnd_, SW_SHOW); InvalidateRect(hwnd_, nullptr, FALSE);
}
void OverlayWindow::show_processing(const std::string&) {
    state_ = 2; InvalidateRect(hwnd_, nullptr, FALSE);
}
void OverlayWindow::show_result(const std::string&) {
    state_ = 3; InvalidateRect(hwnd_, nullptr, FALSE);
    int gen = hide_generation_;
    std::thread([this, gen]() {
        Sleep(2000);
        if (hide_generation_ == gen) hide(); // only hide if no new recording started
    }).detach();
}
void OverlayWindow::hide() {
    state_ = 0; ShowWindow(hwnd_, SW_HIDE);
}
void OverlayWindow::set_audio_level(float db) {
    db_ = db;
    if (state_ == 1) InvalidateRect(hwnd_, nullptr, FALSE);
}

// ─── Paint ─────────────────────────────────────────────

void OverlayWindow::paint(HDC hdc) {
    RECT rc; GetClientRect(hwnd_, &rc);
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, bg); DeleteObject(bg);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    int cy = BAR_MAX + DOT_R + PAD; // center y of dots
    int start_x = PAD + DOT_R;

    ARGB dot_color = pure_mode_ ? 0xC8FFFFFF : 0xC8FFFFFF; // cyan vs white
    Color idleClr(pure_mode_ ? 200 : 200, pure_mode_ ? 150 : 255, pure_mode_ ? 255 : 255, 255);

    if (state_ == 0) {
        SolidBrush dot(idleClr);
        for (int i = 0; i < N_DOTS; ++i) {
            int cx = start_x + i * (DOT_R*2 + GAP);
            g.FillEllipse(&dot, cx - DOT_R, cy - DOT_R, DOT_R*2, DOT_R*2);
        }
    } else if (state_ == 1) {
        float level = std::max(0.0f, std::min(1.0f, (db_ + 48.0f) / 30.0f));
        Color recClr(240, pure_mode_ ? 150 : 255, pure_mode_ ? 255 : 255, 255);
        SolidBrush white(recClr);

        for (int i = 0; i < N_DOTS; ++i) {
            int cx = start_x + i * (DOT_R*2 + GAP);

            float rf = 0.5f + 1.0f * (float)rand() / RAND_MAX;
            float target_h = level * rf * BAR_MAX;
            dot_y_[i] += (target_h - dot_y_[i]) * 0.35f;
            int h = std::max(DOT_R, (int)dot_y_[i]);
            int rr = DOT_R;

            // Dot at rest → cylinder when tall
            if (h <= rr * 2) {
                g.FillEllipse(&white, cx - rr, cy - rr, rr*2, rr*2);
            } else {
                int top = cy - h;
                g.FillEllipse(&white, cx - rr, top, rr*2, rr*2);
                g.FillEllipse(&white, cx - rr, top + h - rr*2, rr*2, rr*2);
                g.FillRectangle(&white, cx - rr, top + rr, rr*2, h - rr*2);
            }
        }
    } else if (state_ == 2) {
        SolidBrush dim(Color(120, 255, 255, 255));
        for (int i = 0; i < N_DOTS; ++i) {
            int cx = start_x + i * (DOT_R*2 + GAP);
            g.FillEllipse(&dim, cx - DOT_R, cy - DOT_R, DOT_R*2, DOT_R*2);
        }
    } else if (state_ == 3) {
        SolidBrush done(Color(180, 255, 255, 255));
        for (int i = 0; i < N_DOTS; ++i) {
            int cx = start_x + i * (DOT_R*2 + GAP);
            g.FillEllipse(&done, cx - DOT_R, cy - DOT_R, DOT_R*2, DOT_R*2);
        }
    }
}

// ─── Window proc ───────────────────────────────────────

LRESULT CALLBACK OverlayWindow::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        // Double-buffer to eliminate flicker
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
        if (self) self->paint(memDC);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        self->drag_x_ = static_cast<short>(LOWORD(lp));
        self->drag_y_ = static_cast<short>(HIWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == hwnd && (wp & MK_LBUTTON)) {
            short nx = static_cast<short>(LOWORD(lp));
            short ny = static_cast<short>(HIWORD(lp));
            RECT r; GetWindowRect(hwnd, &r);
            SetWindowPos(hwnd, nullptr,
                r.left + nx - self->drag_x_,
                r.top + ny - self->drag_y_,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace vim
