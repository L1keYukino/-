#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "src/config/config.hpp"
#include "src/core/events.hpp"
#include "src/core/state_machine.hpp"
#include "src/llm/llm_types.hpp"
#include "src/llm/context_manager.hpp"
#include "src/llm/prompt_templates.hpp"

namespace vim {

class IAudioCapture;
class IASREngine;
class ILLMEngine;
class ITextOutput;
class IHotkeyManager;
class PortAudioCapture;
class SherpaOnnxASR;
class LlamaCppEngine;
class OpenAIEngine;

class VoiceEngine {
public:
    explicit VoiceEngine(const EngineConfig& config);
    ~VoiceEngine();

    // ─── Lifecycle ──────────────────────────────────────
    bool initialize();
    void shutdown();
    bool is_initialized() const;

    // ─── Observers ──────────────────────────────────────
    void add_observer(ObserverPtr observer);
    void remove_observer(ObserverPtr observer);

    // ─── Mode control ───────────────────────────────────
    void start();
    void stop();

    // ─── PTT control ────────────────────────────────────
    void ptt_press();
    void ptt_release();

    // ─── Intent control ─────────────────────────────────
    void set_intent(IntentType intent);

    // ─── State ──────────────────────────────────────────
    EngineState state() const;
    const EngineConfig& config() const { return config_; }

    // ─── Subsystem access ───────────────────────────────
    IAudioCapture*   audio_capture()   const;
    IASREngine*      asr_engine()      const;
    ILLMEngine*      llm_engine()      const;
    ITextOutput*     text_output()     const;
    IHotkeyManager*  hotkey_manager()  const;

private:
    // ─── Event dispatch ─────────────────────────────────
    void notify_state_change(EngineState from, EngineState to, const std::string& detail = "");
    void notify_transcription(const std::string& text, bool partial, float confidence, const std::string& lang);
    void notify_error(int code, const std::string& msg, const std::string& component, bool recoverable);
    void notify_llm_output(const std::string& raw, const std::string& corrected,
                           const std::string& formatted, IntentType intent, bool streaming);

    // ─── Pipeline stages ────────────────────────────────
    static void on_asr_result_static(const struct ASRResult& result, void* user_data);
    void on_recognition_result(const ASRResult& result);
    void drain_audio_and_recognize();
    void run_error_correction(const std::string& raw_text);
    void run_intent_formatting(const std::string& corrected_text);

    // ─── Internal state ─────────────────────────────────
    EngineConfig        config_;
    EngineStateMachine  state_machine_;
    IntentType          current_intent_;

    std::unique_ptr<PortAudioCapture> audio_;
    std::unique_ptr<SherpaOnnxASR>    asr_primary_;
    std::unique_ptr<ILLMEngine>       llm_primary_;
    std::unique_ptr<ITextOutput>      output_;
    std::unique_ptr<IHotkeyManager>   hotkey_;

    PromptCatalog       prompt_catalog_;
    ContextManager      context_mgr_;

    std::mutex observers_mutex_;
    std::vector<ObserverPtr> observers_;

    std::thread asr_thread_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> ptt_active_{false};
    std::atomic<bool> asr_running_{false};
};

} // namespace vim
