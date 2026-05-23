#include "src/ui/settings_dialog.hpp"
#include "src/config/config.hpp"
#include <spdlog/spdlog.h>

namespace vim {

struct SettingsData {
    EngineConfig* config;
    HWND hKey;
    HWND hMic;
    bool saved = false;
};

static LRESULT CALLBACK settings_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = reinterpret_cast<SettingsData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        data = reinterpret_cast<SettingsData*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));

        HINSTANCE hi = cs->hInstance;
        int y = 15;

        // Title
        CreateWindowW(L"STATIC", L"设置", WS_CHILD | WS_VISIBLE,
                      15, y, 350, 22, hwnd, nullptr, hi, nullptr);
        y += 30;

        // API Key
        CreateWindowW(L"STATIC", L"DeepSeek API Key:", WS_CHILD | WS_VISIBLE,
                      15, y, 120, 20, hwnd, nullptr, hi, nullptr);
        data->hKey = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
                       15, y + 20, 350, 24, hwnd, nullptr, hi, nullptr);
        std::wstring wkey(data->config->llm_fallback.api_key.begin(),
                          data->config->llm_fallback.api_key.end());
        SetWindowTextW(data->hKey, wkey.c_str());
        y += 65;

        // Microphone
        CreateWindowW(L"STATIC", L"麦克风设备:", WS_CHILD | WS_VISIBLE,
                      15, y, 100, 20, hwnd, nullptr, hi, nullptr);
        data->hMic = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                       15, y + 20, 350, 200, hwnd, nullptr, hi, nullptr);
        SendMessageW(data->hMic, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"默认设备"));
        SendMessageW(data->hMic, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"麦克风 (HECATE G6 PRO)"));
        SendMessageW(data->hMic, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"麦克风 (Realtek Audio)"));
        SendMessageW(data->hMic, CB_SETCURSEL, 0, 0);
        y += 65;

        // Buttons
        CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      200, y, 80, 28, hwnd, reinterpret_cast<HMENU>(1), hi, nullptr);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      290, y, 80, 28, hwnd, reinterpret_cast<HMENU>(2), hi, nullptr);

        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) { // Save
            wchar_t buf[512];
            GetWindowTextW(data->hKey, buf, 512);
            std::wstring w(buf);
            data->config->llm_fallback.api_key = std::string(w.begin(), w.end());
            data->saved = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wp) == 2) { // Cancel
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool show_settings_dialog(HINSTANCE hinst, HWND parent, EngineConfig& config) {
    SettingsData data;
    data.config = &config;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = settings_wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"VIM_Settings";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"VIM_Settings", L"语音输入法 - 设置",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 400, 280,
                                 parent, nullptr, hinst, &data);

    // Modal loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

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
