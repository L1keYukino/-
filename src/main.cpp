#include "src/core/engine.hpp"
#include "src/output/sendinput_output.hpp"
#include "src/hotkey/win32_hotkey.hpp"
#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <thread>
#include <spdlog/spdlog.h>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace {

vim::VoiceEngine* g_engine = nullptr;
vim::SendInputOutput* g_output = nullptr;
std::string g_last_formatted_text;

void on_signal(int sig) {
    spdlog::info("Signal {} received, shutting down...", sig);
    if (g_engine) {
        g_engine->stop();
        g_engine->shutdown();
    }
    std::exit(0);
}

// Observer that logs events AND auto-types LLM output
struct AutoTypeObserver : public vim::IEngineObserver {
    void on_state_change(const vim::StateChangeEvent& ev) override {
        spdlog::info("[state] {} → {}  {}", vim::engine_state_name(ev.old_state),
                     vim::engine_state_name(ev.new_state), ev.detail);
    }
    void on_audio_level(const vim::AudioLevelEvent& ev) override {
        spdlog::debug("[audio] peak={:.1f} dB", ev.peak_db);
    }
    void on_transcription(const vim::TranscriptionEvent& ev) override {
        spdlog::info("[asr] {} '{}'", ev.is_partial ? "partial" : "final", ev.text);
    }
    void on_llm_output(const vim::LLMOutputEvent& ev) override {
        spdlog::info("[llm] formatted: '{}'", ev.formatted_text);
        g_last_formatted_text = ev.formatted_text;

        // Auto-type into active window
        if (g_output && !ev.formatted_text.empty()) {
            g_output->type_text(ev.formatted_text);
            spdlog::info("[output] typed {} chars", ev.formatted_text.size());
        }
    }
    void on_error(const vim::ErrorEvent& ev) override {
        spdlog::error("[error:{}] {} in {}", ev.code, ev.message, ev.component);
    }
};

} // anonymous namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // Set console to UTF-8 to avoid garbled Chinese output
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    spdlog::set_level(spdlog::level::info);

    spdlog::info("=== Voice Input Method v0.3.0 ===");
    spdlog::info("LLM-driven speech-to-text input method");

    // Parse command line
    const char* config_path = "config/default_config.json";
    bool force_continuous = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--continuous") == 0)
            force_continuous = true;
        else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            config_path = argv[++i];
    }

    // Load config
    vim::EngineConfig config = vim::make_default_config();
    try {
        config = vim::load_config_from_file(config_path);
        spdlog::info("Loaded config from {}", config_path);
    } catch (const std::exception& e) {
        spdlog::warn("Could not load {}: {}", config_path, e.what());
    }

    if (force_continuous) {
        config.mode.mode = vim::EngineMode::Continuous;
        spdlog::info("Forced continuous dictation mode");
    }

    // Init engine
    vim::VoiceEngine engine(config);
    g_engine = &engine;

    if (!engine.initialize()) {
        spdlog::error("Engine initialization failed");
        return 1;
    }

    // Wire output
    vim::SendInputOutput output;
    output.set_typing_delay(config.output.typing_delay_ms);
    g_output = &output;

    // Wire hotkey
    auto hotkey = std::make_unique<vim::Win32HotkeyManager>();

    // Register observer (auto-types LLM output)
    auto observer = std::make_shared<AutoTypeObserver>();
    engine.add_observer(observer);

    // Signals
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    // Start engine
    engine.start();

    if (config.mode.mode == vim::EngineMode::Continuous) {
        spdlog::info("Continuous dictation mode — speak freely");
        spdlog::info("Press Ctrl+C to stop");
    } else {
        // Register PTT hotkey
        vim::HotkeyMod mods = vim::HotkeyMod::Ctrl | vim::HotkeyMod::Alt;
        bool hk_ok = hotkey->register_hotkey(mods, config.mode.ptt_hotkey.virtual_key,
            [&engine](bool start_recording) {
                if (start_recording) {
                    spdlog::info("PTT: recording started (speak now)");
                    engine.ptt_press();
                } else {
                    spdlog::info("PTT: recording stopped (processing)");
                    engine.ptt_release();
                }
            });

        if (hk_ok) {
            spdlog::info("PTT mode — hold Ctrl+Alt+V to dictate");
            spdlog::info("Press Ctrl+C to stop");
        } else {
            spdlog::error("Failed to register hotkey Ctrl+Alt+V");
            spdlog::info("Running in console mode — press Ctrl+C to stop");
        }
    }

    // Main loop: pump Win32 messages
    while (true) {
        if (hotkey) hotkey->pump_messages();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
