#include "src/core/engine.hpp"
#include "src/audio/portaudio_capture.hpp"
#include "src/asr/sherpa_onnx_asr.hpp"
#include "src/llm/i_llm_engine.hpp"
#include "src/output/i_text_output.hpp"
#include "src/hotkey/i_hotkey_manager.hpp"
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>

namespace vim {

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

    spdlog::info("VoiceEngine: initializing subsystems...");

    // ─── Audio capture ──────────────────────────────────
    audio_ = std::make_unique<PortAudioCapture>();
    auto devices = audio_->enumerate_devices();
    if (devices.empty()) {
        spdlog::warn("No audio input devices found");
    } else {
        spdlog::info("Found {} input device(s):", devices.size());
        for (const auto& d : devices)
            spdlog::info("  [{}] {} ({} Hz, {} ch)", d.id, d.name,
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
        spdlog::warn("ASR engine failed to initialize — stub mode");
    }

    asr_primary_->set_result_callback(on_asr_result_static, this);

    initialized_.store(true);
    spdlog::info("VoiceEngine: initialized (Phase 2: audio + ASR)");
    return true;
}

void VoiceEngine::shutdown() {
    if (!initialized_.load()) return;

    spdlog::info("VoiceEngine: shutting down...");

    asr_running_.store(false);
    if (asr_thread_.joinable())
        asr_thread_.join();

    if (audio_ && audio_->is_running())
        audio_->stop();

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
    spdlog::info("VoiceEngine: started in {} mode",
                 config_.mode.mode == EngineMode::PTT ? "PTT" : "Continuous");
    state_machine_.force_state(EngineState::Idle);
}

void VoiceEngine::stop() {
    if (ptt_active_.load()) ptt_release();
    state_machine_.force_state(EngineState::Idle);
    spdlog::info("VoiceEngine: stopped");
}

// ─── PTT control ────────────────────────────────────────

void VoiceEngine::ptt_press() {
    if (!initialized_.load()) return;
    if (ptt_active_.exchange(true)) return;

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Listening);
    notify_state_change(prev, EngineState::Listening);

    audio_->ring_buffer().reset();

    if (!audio_->start(config_.audio.sample_rate,
                       config_.audio.channels,
                       nullptr, nullptr)) {
        notify_error(-2, "Failed to start audio capture", "audio", true);
        ptt_active_.store(false);
        return;
    }

    spdlog::debug("PTT: recording started ({} Hz, {} ch)",
                  config_.audio.sample_rate, config_.audio.channels);
}

void VoiceEngine::ptt_release() {
    if (!ptt_active_.exchange(false)) return;

    audio_->stop();

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Recognizing);
    notify_state_change(prev, EngineState::Recognizing);

    std::size_t buffered = audio_->ring_buffer().read_available();
    spdlog::debug("PTT: recording stopped, {} samples in buffer ({:.1f} seconds)",
                  buffered,
                  static_cast<float>(buffered) / static_cast<float>(config_.audio.sample_rate));

    drain_audio_and_recognize();
}

// ─── Intent control ─────────────────────────────────────

void VoiceEngine::set_intent(IntentType intent) {
    current_intent_ = intent;
    spdlog::debug("Intent set to {}", intent_type_name(intent));
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
        spdlog::debug("ASR partial: {}", result.text);
        notify_transcription(result.text, true, result.confidence, result.language);
        return;
    }

    spdlog::info("ASR final: '{}' (confidence: {:.2f})", result.text, result.confidence);

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Correcting);
    notify_state_change(prev, EngineState::Correcting, "ASR result received");

    notify_transcription(result.text, false, result.confidence, result.language);

    auto prev2 = state_machine_.state();
    state_machine_.transition_to(EngineState::Outputting);
    notify_state_change(prev2, EngineState::Outputting);

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
        spdlog::warn("No audio data captured — skipping recognition");
        state_machine_.transition_to(EngineState::Idle);
        return;
    }

    std::vector<float> chunk(available);
    std::size_t read = rb.read(chunk.data(), available);
    chunk.resize(read);

    spdlog::debug("Drained {} samples ({:.1f} seconds) from ring buffer",
                  read,
                  static_cast<float>(read) / static_cast<float>(config_.audio.sample_rate));

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
