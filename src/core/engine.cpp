#include "src/core/engine.hpp"
#include "src/audio/portaudio_capture.hpp"
#include "src/asr/sherpa_onnx_asr.hpp"
#include "src/llm/i_llm_engine.hpp"
#include "src/output/i_text_output.hpp"
#include "src/hotkey/i_hotkey_manager.hpp"
#include <algorithm>
#include <cstdio>
#include <chrono>

namespace vim {

// ─── Logging helper (spdlog replacement until deps are available) ─

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
    template<typename... Args>
    void log_warn(const char* fmt, Args... args) {
        std::fprintf(stderr, "[vim:warn] ");
        std::fprintf(stderr, fmt, args...);
        std::fprintf(stderr, "\n");
    }
}

// ─── Constructor / Destructor ───────────────────────────

VoiceEngine::VoiceEngine(const EngineConfig& config)
    : config_(config)
    , current_intent_(config.default_intent)
{
}

VoiceEngine::~VoiceEngine() {
    shutdown();
}

// ─── Lifecycle ──────────────────────────────────────────

bool VoiceEngine::initialize() {
    if (initialized_.load()) return true;

    log_info("VoiceEngine: initializing subsystems...");

    // ─── Audio capture ──────────────────────────────────
    audio_ = std::make_unique<PortAudioCapture>();
    auto devices = audio_->enumerate_devices();
    if (devices.empty()) {
        log_warn("No audio input devices found");
    } else {
        log_info("Found %zu input device(s):", devices.size());
        for (const auto& d : devices)
            log_info("  [%s] %s (%d Hz, %d ch)", d.id.c_str(), d.name.c_str(),
                     d.default_sample_rate, d.max_input_channels);
        audio_->select_device(config_.audio.device_id);
    }

    // ─── ASR engine ─────────────────────────────────────
    asr_primary_ = std::make_unique<SherpaOnnxASR>();
    ASREngineConfig asr_cfg;
    asr_cfg.model_dir     = config_.asr_primary.model_dir;
    asr_cfg.sample_rate   = config_.audio.sample_rate;
    asr_cfg.enable_vad    = config_.asr_primary.enable_vad;
    asr_cfg.vad_silence_timeout_s = config_.asr_primary.vad_silence_timeout_s;
    asr_cfg.vad_speech_timeout_s  = config_.asr_primary.vad_speech_timeout_s;

    if (!asr_primary_->initialize(asr_cfg)) {
        log_warn("ASR engine failed to initialize — stub mode");
    }

    // Wire ASR result callback
    asr_primary_->set_result_callback(on_asr_result_static, this);

    // ─── Done ───────────────────────────────────────────
    initialized_.store(true);
    log_info("VoiceEngine: initialized (Phase 2: audio + ASR)");
    return true;
}

void VoiceEngine::shutdown() {
    if (!initialized_.load()) return;

    log_info("VoiceEngine: shutting down...");

    // Stop ASR thread
    asr_running_.store(false);
    if (asr_thread_.joinable())
        asr_thread_.join();

    // Stop audio
    if (audio_ && audio_->is_running())
        audio_->stop();

    // Reset subsystems
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

// ─── Observer management ────────────────────────────────

void VoiceEngine::add_observer(ObserverPtr observer) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    observers_.push_back(std::move(observer));
}

void VoiceEngine::remove_observer(ObserverPtr observer) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    auto it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end()) observers_.erase(it);
}

// ─── Mode control ───────────────────────────────────────

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

// ─── PTT control ────────────────────────────────────────

void VoiceEngine::ptt_press() {
    if (!initialized_.load()) return;
    if (ptt_active_.exchange(true)) return; // already active

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Listening);
    notify_state_change(prev, EngineState::Listening);

    // Reset ring buffer and start capture
    audio_->ring_buffer().reset();

    if (!audio_->start(config_.audio.sample_rate,
                       config_.audio.channels,
                       nullptr, nullptr)) {
        notify_error(-2, "Failed to start audio capture", "audio", true);
        ptt_active_.store(false);
        return;
    }

    log_debug("PTT: recording started (%d Hz, %d ch)",
              config_.audio.sample_rate, config_.audio.channels);
}

void VoiceEngine::ptt_release() {
    if (!ptt_active_.exchange(false)) return;

    // Stop audio capture immediately
    audio_->stop();

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Recognizing);
    notify_state_change(prev, EngineState::Recognizing);

    // Drain ring buffer → ASR
    std::size_t buffered = audio_->ring_buffer().read_available();
    log_debug("PTT: recording stopped, %zu samples in buffer (%.1f seconds)",
              buffered, static_cast<float>(buffered) / static_cast<float>(config_.audio.sample_rate));

    drain_audio_and_recognize();
}

// ─── Intent control ─────────────────────────────────────

void VoiceEngine::set_intent(IntentType intent) {
    current_intent_ = intent;
    log_debug("Intent set to %s", intent_type_name(intent));
}

EngineState VoiceEngine::state() const {
    return state_machine_.state();
}

// ─── Subsystem access ───────────────────────────────────

IAudioCapture*  VoiceEngine::audio_capture()  const { return audio_.get(); }
IASREngine*     VoiceEngine::asr_engine()     const { return asr_primary_.get(); }
ILLMEngine*     VoiceEngine::llm_engine()     const { return llm_primary_.get(); }
ITextOutput*    VoiceEngine::text_output()    const { return output_.get(); }
IHotkeyManager* VoiceEngine::hotkey_manager() const { return hotkey_.get(); }

// ─── Pipeline: ASR result → event dispatch ──────────────

void VoiceEngine::on_asr_result_static(const ASRResult& result, void* user_data) {
    auto* self = static_cast<VoiceEngine*>(user_data);
    self->on_recognition_result(result);
}

void VoiceEngine::on_recognition_result(const ASRResult& result) {
    if (result.is_partial) {
        log_debug("ASR partial: %s", result.text.c_str());
        notify_transcription(result.text, true, result.confidence, result.language);
        return;
    }

    log_info("ASR final: '%s' (confidence: %.2f)", result.text.c_str(),
             static_cast<double>(result.confidence));

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Correcting);
    notify_state_change(prev, EngineState::Correcting,
                        "ASR result received");

    notify_transcription(result.text, false, result.confidence, result.language);

    // Phase 3: feed to LLM for correction + formatting
    // For now (Phase 2), the raw ASR text is the final output.
    auto prev2 = state_machine_.state();
    state_machine_.transition_to(EngineState::Outputting);
    notify_state_change(prev2, EngineState::Outputting);

    // Notify with raw text as both corrected and formatted (no LLM yet)
    notify_llm_output(result.text, result.text, result.text,
                      current_intent_, false);

    state_machine_.transition_to(EngineState::Idle);
    notify_state_change(EngineState::Outputting, EngineState::Idle);
}

// ─── Pipeline: ring buffer → ASR ────────────────────────

void VoiceEngine::drain_audio_and_recognize() {
    auto& rb = audio_->ring_buffer();
    std::size_t available = rb.read_available();

    if (available == 0) {
        log_warn("No audio data captured — skipping recognition");
        state_machine_.transition_to(EngineState::Idle);
        return;
    }

    // Read all samples from ring buffer
    std::vector<float> chunk(available);
    std::size_t read = rb.read(chunk.data(), available);
    chunk.resize(read);

    log_debug("Drained %zu samples (%.1f seconds) from ring buffer",
              read, static_cast<float>(read) / static_cast<float>(config_.audio.sample_rate));

    // Feed to ASR and trigger final decode
    asr_primary_->process_audio(chunk.data(), chunk.size());
    asr_primary_->end_utterance();
}

// ─── Event dispatch ─────────────────────────────────────

void VoiceEngine::notify_state_change(EngineState from, EngineState to,
                                       const std::string& detail) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    StateChangeEvent ev{from, to, detail};
    for (auto& obs : observers_)
        obs->on_state_change(ev);
}

void VoiceEngine::notify_transcription(const std::string& text, bool partial,
                                        float confidence, const std::string& lang) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    TranscriptionEvent ev{text, partial, confidence, lang};
    for (auto& obs : observers_)
        obs->on_transcription(ev);
}

void VoiceEngine::notify_error(int code, const std::string& msg,
                                const std::string& component, bool recoverable) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    ErrorEvent ev{code, msg, component, recoverable};
    for (auto& obs : observers_)
        obs->on_error(ev);
}

void VoiceEngine::notify_llm_output(const std::string& raw, const std::string& corrected,
                                     const std::string& formatted, IntentType intent,
                                     bool streaming) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    LLMOutputEvent ev{raw, corrected, formatted, intent, streaming};
    for (auto& obs : observers_)
        obs->on_llm_output(ev);
}

} // namespace vim
