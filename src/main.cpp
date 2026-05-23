#include "src/core/engine.hpp"
#include "src/output/sendinput_output.hpp"
#include "src/hotkey/win32_hotkey.hpp"
#include <csignal>
#include <cstdio>
#include <thread>
#include <spdlog/spdlog.h>

namespace {

vim::VoiceEngine* g_engine = nullptr;

void on_signal(int sig) {
    spdlog::info("Signal {} received, shutting down...", sig);
    if (g_engine) {
        g_engine->stop();
        g_engine->shutdown();
    }
    std::exit(0);
}

// Simple observer that logs events to console
struct ConsoleObserver : public vim::IEngineObserver {
    void on_state_change(const vim::StateChangeEvent& ev) override {
        spdlog::info("[state] {} → {}  {}", vim::engine_state_name(ev.old_state),
                     vim::engine_state_name(ev.new_state), ev.detail);
    }
    void on_audio_level(const vim::AudioLevelEvent& ev) override {
        spdlog::debug("[audio] peak={:.1f} dB, rms={:.1f} dB", ev.peak_db, ev.rms_db);
    }
    void on_transcription(const vim::TranscriptionEvent& ev) override {
        spdlog::info("[asr] {} '{}' (conf={:.2f})",
                     ev.is_partial ? "partial" : "final", ev.text, ev.confidence);
    }
    void on_llm_output(const vim::LLMOutputEvent& ev) override {
        spdlog::info("[llm] raw='{}' → corrected='{}' → formatted='{}' (intent={})",
                     ev.raw_text, ev.corrected_text, ev.formatted_text,
                     vim::intent_type_name(ev.intent));
    }
    void on_error(const vim::ErrorEvent& ev) override {
        spdlog::error("[error:{}] {} in {} (recoverable={})",
                      ev.code, ev.message, ev.component, ev.is_recoverable);
    }
};

} // anonymous namespace

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);

    spdlog::info("=== Voice Input Method v0.2.0 ===");
    spdlog::info("LLM-driven speech-to-text input method");

    // Load config
    vim::EngineConfig config = vim::make_default_config();
    const char* config_path = "config/default_config.json";

    if (argc > 1) config_path = argv[1];

    try {
        config = vim::load_config_from_file(config_path);
        spdlog::info("Loaded config from {}", config_path);
    } catch (const std::exception& e) {
        spdlog::warn("Could not load config from {}: {}", config_path, e.what());
        spdlog::info("Using default configuration");
    }

    // Initialize engine
    vim::VoiceEngine engine(config);
    g_engine = &engine;

    if (!engine.initialize()) {
        spdlog::error("Engine initialization failed");
        return 1;
    }

    // Wire output (SendInput)
    auto output = std::make_unique<vim::SendInputOutput>();
    output->set_typing_delay(config.output.typing_delay_ms);
    // The engine now owns the output backend (via factory in Phase 5)

    // Wire hotkey (PTT)
    auto hotkey = std::make_unique<vim::Win32HotkeyManager>();

    // Register observer
    auto observer = std::make_shared<ConsoleObserver>();
    engine.add_observer(observer);

    // Set up signal handlers
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    // Register PTT hotkey (Ctrl+Alt+V by default)
    vim::HotkeyMod mods = vim::HotkeyMod::Ctrl | vim::HotkeyMod::Alt;
    bool hk_ok = hotkey->register_hotkey(mods, config.mode.ptt_hotkey.virtual_key,
        [&engine](bool key_down) {
            if (key_down) {
                spdlog::info("PTT: key pressed");
                engine.ptt_press();
            } else {
                spdlog::info("PTT: key released");
                engine.ptt_release();

                // Type the formatted text into the active window
                // (Phase 3: LLM result is available via observer)
                // Phase 4+: auto-type via SendInput
            }
        });

    if (!hk_ok) {
        spdlog::error("Failed to register hotkey. Is another app using Ctrl+Alt+V?");
        spdlog::info("Running in console mode — type '/quit' to exit");
    } else {
        spdlog::info("Press Ctrl+Alt+V to start dictation");
        spdlog::info("Type '/quit' to exit");
    }

    engine.start();

    // Main loop: pump messages + handle console input
    std::string line;
    while (true) {
        // Pump Win32 messages (hotkey)
        if (hotkey) hotkey->pump_messages();

        // Check for console input (non-blocking on Windows with kbhit)
        // For simplicity: just pump and sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
