#include "src/core/engine.hpp"
#include "src/audio/i_audio_capture.hpp"
#include "src/asr/i_asr_engine.hpp"
#include "src/llm/i_llm_engine.hpp"
#include "src/output/i_text_output.hpp"
#include "src/hotkey/i_hotkey_manager.hpp"
#include <algorithm>
#include <cstdio>

namespace vim {

// ─── Minimal logging (spdlog replacement until Phase 2) ─

namespace {
    template<typename... Args>
    void log_info(const char* fmt, Args... args) {
        std::fprintf(stderr, "[vim:info] ");
        std::fprintf(stderr, fmt, args...);
        std::fprintf(stderr, "\n");
    }
    template<typename... Args>
    void log_debug(const char* fmt, Args... args) {
        std::fprintf(stderr, "[vim:debug] ");
        std::fprintf(stderr, fmt, args...);
        std::fprintf(stderr, "\n");
    }
}

VoiceEngine::VoiceEngine(const EngineConfig& config)
    : config_(config)
    , current_intent_(config.default_intent)
{
}

VoiceEngine::~VoiceEngine() {
    shutdown();
}

bool VoiceEngine::initialize() {
    if (initialized_.load()) return true;

    log_info("VoiceEngine: initializing...");

    initialized_.store(true);
    log_info("VoiceEngine: initialized (skeleton mode — no backends loaded)");
    return true;
}

void VoiceEngine::shutdown() {
    if (!initialized_.load()) return;

    log_info("VoiceEngine: shutting down...");

    if (audio_ && audio_->is_running())
        audio_->stop();

    if (hotkey_)
        hotkey_->unregister_hotkey();

    audio_.reset();
    asr_primary_.reset();
    llm_primary_.reset();
    output_.reset();
    hotkey_.reset();

    state_machine_.force_state(EngineState::Idle);
    initialized_.store(false);
}

bool VoiceEngine::is_initialized() const {
    return initialized_.load();
}

void VoiceEngine::add_observer(ObserverPtr observer) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    observers_.push_back(std::move(observer));
}

void VoiceEngine::remove_observer(ObserverPtr observer) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    auto it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end()) observers_.erase(it);
}

void VoiceEngine::start() {
    if (!initialized_.load()) {
        notify_error(-1, "Engine not initialized", "core", true);
        return;
    }
    log_info("VoiceEngine: started in %s mode",
             config_.mode.mode == EngineMode::PTT ? "PTT" : "Continuous");
    state_machine_.force_state(EngineState::Idle);
}

void VoiceEngine::stop() {
    if (ptt_active_.load()) ptt_release();
    state_machine_.force_state(EngineState::Idle);
    log_info("VoiceEngine: stopped");
}

void VoiceEngine::ptt_press() {
    if (!initialized_.load()) return;
    if (ptt_active_.exchange(true)) return;

    state_machine_.transition_to(EngineState::Listening);
    audio_buffer_.clear();
    log_debug("PTT: recording started");
}

void VoiceEngine::ptt_release() {
    if (!ptt_active_.exchange(false)) return;

    state_machine_.transition_to(EngineState::Recognizing);
    log_debug("PTT: recording stopped, %zu samples buffered", audio_buffer_.size());
    dump_audio_and_recognize();
}

void VoiceEngine::set_intent(IntentType intent) {
    current_intent_ = intent;
    log_debug("Intent set to %s", intent_type_name(intent));
}

EngineState VoiceEngine::state() const {
    return state_machine_.state();
}

// ─── Private ────────────────────────────────────────────

void VoiceEngine::notify_state_change(EngineState from, EngineState to, const std::string& detail) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    StateChangeEvent ev{from, to, detail};
    for (auto& obs : observers_)
        obs->on_state_change(ev);
}

void VoiceEngine::notify_error(int code, const std::string& msg,
                                const std::string& component, bool recoverable) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    ErrorEvent ev{code, msg, component, recoverable};
    for (auto& obs : observers_)
        obs->on_error(ev);
}

void VoiceEngine::notify_llm_output(const std::string& raw, const std::string& corrected,
                                     const std::string& formatted, IntentType intent, bool streaming) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    LLMOutputEvent ev{raw, corrected, formatted, intent, streaming};
    for (auto& obs : observers_)
        obs->on_llm_output(ev);
}

void VoiceEngine::on_recognition_result(const ASRResult& result) {
    if (result.is_partial) {
        log_debug("ASR partial: %s", result.text.c_str());
        return;
    }
    log_info("ASR final: '%s' (confidence: %.2f)", result.text.c_str(), result.confidence);
}

void VoiceEngine::dump_audio_and_recognize() {
    log_debug("Audio dump placeholder: %zu samples", audio_buffer_.size());
}

} // namespace vim
