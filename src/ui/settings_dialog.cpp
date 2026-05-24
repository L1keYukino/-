#include "src/ui/settings_dialog.hpp"
#include "src/config/config.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace vim {

struct SettingsData {
    EngineConfig* config;
    HWND hHotkeyDisplay;
    HWND hRecordBtn;
    HWND hApiKey;
    HWND hShowViz;
    uint32_t captured_mods = 3;
    uint32_t captured_vk = 0x56;
    bool recording = false;
    bool saved = false;
    bool show_viz = true;
};

static std::wstring hotkey_name(uint32_t mods, uint32_t vk) {
    std::wstring s;
    if (mods & 2) s += L"Ctrl+";
    if (mods & 1) s += L"Alt+";
    if (mods & 4) s += L"Shift+";
    if (mods & 8) s += L"Win+";
    // Mouse buttons
    if (vk == VK_XBUTTON1) s += L"鼠标侧键1(前进)";
    else if (vk == VK_XBUTTON2) s += L"鼠标侧键2(后退)";
    else if (vk == VK_MBUTTON) s += L"鼠标中键";
    else if (vk == VK_RBUTTON) s += L"鼠标右键";
    else {
        wchar_t name[64] = {0};
        UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        GetKeyNameTextW(sc << 16, name, 64);
        if (name[0]) s += name;
        else s += L"VK_" + std::to_wstring(vk);
    }
    return s;
}

static LRESULT CALLBACK settings_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = reinterpret_cast<SettingsData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        data = reinterpret_cast<SettingsData*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));

        HINSTANCE hi = cs->hInstance;
        int y = 15, x = 15;

        // API Key section
        CreateWindowW(L"STATIC", L"DeepSeek API Key:", WS_CHILD | WS_VISIBLE,
                      x, y, 120, 20, hwnd, nullptr, hi, nullptr);
        data->hApiKey = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                       x, y + 20, 340, 24, hwnd, nullptr, hi, nullptr);
        {
            std::wstring w(data->config->llm_fallback.api_key.begin(),
                          data->config->llm_fallback.api_key.end());
            SetWindowTextW(data->hApiKey, w.c_str());
        }
        y += 55;

        // Hotkey section
        CreateWindowW(L"STATIC", L"快捷键设置", WS_CHILD | WS_VISIBLE,
                      x, y, 350, 22, hwnd, nullptr, hi, nullptr);
        y += 25;

        CreateWindowW(L"STATIC", L"当前快捷键:", WS_CHILD | WS_VISIBLE,
                      x, y, 80, 20, hwnd, nullptr, hi, nullptr);
        auto name = hotkey_name(data->captured_mods, data->captured_vk);
        data->hHotkeyDisplay = CreateWindowW(L"STATIC", name.c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER,
                              100, y, 180, 22, hwnd, nullptr, hi, nullptr);
        y += 28;

        data->hRecordBtn = CreateWindowW(L"BUTTON", L"🎤 点击录制新快捷键", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          x, y, 220, 30, hwnd, reinterpret_cast<HMENU>(3), hi, nullptr);
        y += 40;

        // Visualizer toggle
        data->hShowViz = CreateWindowW(L"BUTTON", L"显示音律条", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                        x, y, 150, 22, hwnd, reinterpret_cast<HMENU>(4), hi, nullptr);
        SendMessageW(data->hShowViz, BM_SETCHECK, data->show_viz ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 28;

        CreateWindowW(L"STATIC", L"提示：修改后需重启生效", WS_CHILD | WS_VISIBLE,
                      x, y, 300, 18, hwnd, nullptr, hi, nullptr);
        y += 30;

        CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      200, y, 80, 28, hwnd, reinterpret_cast<HMENU>(1), hi, nullptr);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      290, y, 80, 28, hwnd, reinterpret_cast<HMENU>(2), hi, nullptr);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) { // Save
            wchar_t buf[256];
            GetWindowTextW(data->hApiKey, buf, 256);
            std::wstring w(buf);
            data->config->llm_fallback.api_key = std::string(w.begin(), w.end());
            data->config->llm_fallback.enabled = !data->config->llm_fallback.api_key.empty();
            data->config->mode.ptt_hotkey.modifiers = data->captured_mods;
            data->config->mode.ptt_hotkey.virtual_key = data->captured_vk;
            bool show_viz = (SendMessageW(data->hShowViz, BM_GETCHECK, 0, 0) == BST_CHECKED);
            // Write show_viz to a simple file
            {
                std::ofstream f("config/show_viz.txt");
                if (f) f << (show_viz ? "1" : "0");
            }
            data->saved = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wp) == 2) { DestroyWindow(hwnd); return 0; } // Cancel
        if (LOWORD(wp) == 3) { // Record
            data->recording = true;
            SetWindowTextW(data->hRecordBtn, L"按下组合键... (5秒超时)");
            EnableWindow(data->hRecordBtn, FALSE);
            SetFocus(hwnd);
            // Auto-cancel after 5 seconds
            SetTimer(hwnd, 1, 5000, nullptr);
            return 0;
        }
        break;
    case WM_TIMER:
        if (wp == 1 && data && data->recording) {
            data->recording = false;
            KillTimer(hwnd, 1);
            SetWindowTextW(data->hRecordBtn, L"点击录制新快捷键");
            EnableWindow(data->hRecordBtn, TRUE);
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_XBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        uint32_t vk = static_cast<uint32_t>(wp);
        // For mouse messages, convert to virtual mouse code
        if (msg == WM_XBUTTONDOWN) vk = (GET_XBUTTON_WPARAM(wp) == 1) ? VK_XBUTTON1 : VK_XBUTTON2;
        else if (msg == WM_MBUTTONDOWN) vk = VK_MBUTTON;
        else if (msg == WM_RBUTTONDOWN) vk = VK_RBUTTON;

        // Skip pure modifier keys (keyboard only)
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT ||
                vk == VK_LWIN || vk == VK_RWIN || vk == VK_CAPITAL)
                break;
        }

        if (data && data->recording) {
            uint32_t mods = 0;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= 2;
            if (GetAsyncKeyState(VK_MENU) & 0x8000)    mods |= 1;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   mods |= 4;
            if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) mods |= 8;

            data->captured_mods = mods;
            data->captured_vk = vk;
            data->recording = false;
            KillTimer(hwnd, 1);

            auto name = hotkey_name(mods, vk);
            SetWindowTextW(data->hHotkeyDisplay, name.c_str());
            SetWindowTextW(data->hRecordBtn, L"点击录制新快捷键");
            EnableWindow(data->hRecordBtn, TRUE);
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool show_settings_dialog(HINSTANCE hinst, HWND parent, HWND main_hwnd, EngineConfig& config) {
    SettingsData data;
    data.config = &config;
    data.captured_mods = config.mode.ptt_hotkey.modifiers;
    data.captured_vk = config.mode.ptt_hotkey.virtual_key;

    (void)main_hwnd; // hotkey is polling-based, no unregister needed

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = settings_wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"VIM_Settings";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"VIM_Settings", L"语音输入法 - 设置",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 380, 310,
                                 parent, nullptr, hinst, &data);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Hotkey changes take effect immediately (polling-based)
    if (data.saved) {
        try {
            save_config_to_file("config/default_config.json", config);
            spdlog::info("Settings saved");
            return true;
        } catch (...) {
            spdlog::error("Failed to save config");
        }
    }
    return false;
}

} // namespace vim
