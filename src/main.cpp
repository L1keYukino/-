#include "src/core/engine.hpp"
#include "src/output/sendinput_output.hpp"
#include "src/ui/tray_icon.hpp"
#include "src/ui/overlay_window.hpp"
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

vim::VoiceEngine*      g_engine  = nullptr;
vim::SendInputOutput*  g_output  = nullptr;
vim::TrayIcon*         g_tray    = nullptr;
vim::OverlayWindow*    g_overlay = nullptr;

std::string g_last_formatted;
bool g_recording = false;
bool g_processing = false; // lock during ASR+LLM pipeline

void toggle_recording() {
    if (!g_engine) return;
    if (g_processing) return;

    if (g_recording) {
        g_engine->ptt_release();
    } else {
        g_engine->ptt_press();
    }
}

// Observer: connects engine events to UI
struct UIObserver : public vim::IEngineObserver {
    void on_state_change(const vim::StateChangeEvent& ev) override {
        spdlog::info("[state] {} → {}", vim::engine_state_name(ev.old_state),
                     vim::engine_state_name(ev.new_state));

        if (ev.new_state == vim::EngineState::Listening) {
            g_recording = true;
            if (g_tray)    g_tray->set_recording(true);
            if (g_overlay) g_overlay->show_recording();
        }
        else if (ev.new_state == vim::EngineState::Recognizing) {
            g_recording = false;
            g_processing = true;
            if (g_tray) g_tray->set_recording(false);
        }
        else if (ev.new_state == vim::EngineState::Outputting) {
            if (g_overlay) g_overlay->show_result(g_last_formatted);
        }
        else if (ev.new_state == vim::EngineState::Idle) {
            g_processing = false;
            g_recording = false;
            if (g_tray)    g_tray->set_recording(false);
            // Delay hide so user can see the result/silent notification
            std::thread([]() {
                Sleep(1500);
                if (g_overlay) g_overlay->hide();
            }).detach();
        }
    }

    void on_audio_level(const vim::AudioLevelEvent& ev) override {
        spdlog::debug("[audio] peak={:.1f} dB", ev.peak_db);
    }

    void on_transcription(const vim::TranscriptionEvent& ev) override {
        if (ev.is_partial && g_overlay)
            g_overlay->set_asr_text(ev.text);

        if (!ev.is_partial) {
            spdlog::info("[asr] '{}'", ev.text);
            if (g_overlay) g_overlay->show_processing(ev.text);
        }
    }

    void on_llm_output(const vim::LLMOutputEvent& ev) override {
        spdlog::info("[llm] '{}'", ev.formatted_text);
        g_last_formatted = ev.formatted_text;

        if (g_output && !ev.formatted_text.empty()) {
            g_output->type_text(ev.formatted_text);
            spdlog::info("[output] typed {} chars", ev.formatted_text.size());
        }
    }

    void on_error(const vim::ErrorEvent& ev) override {
        spdlog::error("[error:{}] {} in {}", ev.code, ev.message, ev.component);
    }
};

// Hidden window for tray icon message handling
static const wchar_t* MAIN_WINDOW_CLASS = L"VIM_MainWindow";
static constexpr UINT WM_TRAYICON = WM_APP + 1;
static DWORD last_toggle_ms = 0;

LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_LBUTTONUP) {
            toggle_recording();
        } else if (LOWORD(lp) == WM_RBUTTONUP) {
            if (g_tray) g_tray->show_menu();
        }
        return 0;
    case WM_HOTKEY:
        {
            DWORD now = GetTickCount();
            if (now - last_toggle_ms < 300) return 0;
            last_toggle_ms = now;
        }
        toggle_recording();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // anonymous namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Log to file since this is a GUI app (no console)
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("vim.log", true);
    auto logger = std::make_shared<spdlog::logger>("vim", file_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::info("=== Voice Input Method v0.4.0 (UI) ===");

    // ─── Load config ────────────────────────────────────
    vim::EngineConfig config = vim::make_default_config();
    try {
        config = vim::load_config_from_file("config/default_config.json");
        spdlog::info("Loaded config");
    } catch (...) {
        spdlog::warn("Using default config");
    }

    // ─── Init engine ────────────────────────────────────
    vim::VoiceEngine engine(config);
    g_engine = &engine;

    if (!engine.initialize()) {
        MessageBoxW(nullptr, L"引擎初始化失败", L"语音输入法", MB_ICONERROR);
        return 1;
    }

    // ─── Init output ────────────────────────────────────
    vim::SendInputOutput output;
    output.set_typing_delay(config.output.typing_delay_ms);
    g_output = &output;

    // ─── Register main window class ─────────────────────
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = main_wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, MAIN_WINDOW_CLASS, L"VIM",
                                 WS_OVERLAPPED, 0, 0, 0, 0,
                                 nullptr, nullptr, hInstance, nullptr);

    // ─── Tray icon ──────────────────────────────────────
    vim::TrayIcon tray;
    g_tray = &tray;
    tray.create(hInstance, hwnd);

    tray.set_menu_start_callback([]() {
        if (!g_recording) toggle_recording();
    });
    tray.set_menu_stop_callback([]() {
        if (g_recording) toggle_recording();
    });
    tray.set_menu_quit_callback([]() {
        if (g_engine) { g_engine->stop(); g_engine->shutdown(); }
        PostQuitMessage(0);
    });
    tray.set_left_click_callback([]() { toggle_recording(); });

    // ─── Overlay window ─────────────────────────────────
    vim::OverlayWindow overlay;
    g_overlay = &overlay;
    overlay.create(hInstance);

    // ─── Observer ───────────────────────────────────────
    auto obs = std::make_shared<UIObserver>();
    engine.add_observer(obs);

    // ─── Hotkey (registered directly on main window) ────
    UINT hotkey_mods = MOD_CONTROL | MOD_ALT;
    if (RegisterHotKey(hwnd, 1, hotkey_mods, config.mode.ptt_hotkey.virtual_key)) {
        spdlog::info("Hotkey Ctrl+Alt+V registered on main window");
    } else {
        spdlog::error("RegisterHotKey failed: {}", GetLastError());
    }

    // ─── Start engine ───────────────────────────────────
    engine.start();

    // ─── Message loop ───────────────────────────────────
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // ─── Cleanup ────────────────────────────────────────
    engine.stop();
    engine.shutdown();
    tray.destroy();
    overlay.destroy();

    return 0;
}
