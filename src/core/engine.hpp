#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "src/config/config.hpp"
#include "src/core/events.hpp"
#include "src/core/state_machine.hpp"

namespace vim {

class IAudioCapture;
class IASREngine;
class ILLMEngine;
class ITextOutput;
class IHotkeyManager;

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

    // ─── Subsystem access (for testing / UI) ────────────
    IAudioCapture*   audio_capture()   const { return audio_.get(); }
    IASREngine*      asr_engine()      const { return asr_primary_.get(); }
    ILLMEngine*      llm_engine()      const { return llm_primary_.get(); }
    ITextOutput*     text_output()     const { return output_.get(); }
    IHotkeyManager*  hotkey_manager()  const { return hotkey_.get(); }

private:
    // ─── Event dispatch ─────────────────────────────────
    void notify_state_change(EngineState from, EngineState to, const std::string& detail = "");
    void notify_error(int code, const std::string& msg, const std::string& component, bool recoverable);
    void notify_llm_output(const std::string& raw, const std::string& corrected,
                           const std::string& formatted, IntentType intent, bool streaming);

    // ─── Pipeline stages ────────────────────────────────
    void on_recognition_result(const struct ASRResult& result);
    void dump_audio_and_recognize();

    // ─── Internal state ─────────────────────────────────
    EngineConfig        config_;
    EngineStateMachine  state_machine_;
    std::vector<float>  audio_buffer_;
    IntentType          current_intent_;

    std::unique_ptr<IAudioCapture>   audio_;
    std::unique_ptr<IASREngine>      asr_primary_;
    std::unique_ptr<ILLMEngine>      llm_primary_;
    std::unique_ptr<ITextOutput>     output_;
    std::unique_ptr<IHotkeyManager>  hotkey_;

    std::mutex observers_mutex_;
    std::vector<ObserverPtr> observers_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> ptt_active_{false};
};

} // namespace vim
