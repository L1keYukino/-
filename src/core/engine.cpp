#include "src/core/engine.hpp"
#include "src/audio/portaudio_capture.hpp"
#include "src/asr/sherpa_onnx_asr.hpp"
#include "src/asr/iflytek_cloud_asr.hpp"
#include "src/asr/asr_fallback.hpp"
#include "src/llm/llamacpp_engine.hpp"
#include "src/llm/openai_engine.hpp"
#include "src/llm/llm_fallback.hpp"
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
    , context_mgr_(config.max_context_turns)
    , vad_(EnergyVAD::Config{
          config_.asr_primary.vad_silence_timeout_s > 0
              ? -40.0f : -40.0f,  // silence threshold
          -30.0f,                  // speech threshold
          config_.mode.continuous_vad_silence_ms,
          100,                     // speech confirm ms
          config_.audio.sample_rate
      })
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

    // ─── ASR engine (primary: sherpa-onnx, fallback: iFlytek) ──
    auto primary_asr = std::make_unique<SherpaOnnxASR>();
    ASREngineConfig asr_cfg;
    asr_cfg.model_dir     = config_.asr_primary.model_dir;
    asr_cfg.sample_rate   = config_.audio.sample_rate;
    asr_cfg.enable_vad    = config_.asr_primary.enable_vad;
    asr_cfg.vad_silence_timeout_s = config_.asr_primary.vad_silence_timeout_s;
    asr_cfg.vad_speech_timeout_s  = config_.asr_primary.vad_speech_timeout_s;
    primary_asr->initialize(asr_cfg);

    std::unique_ptr<IASREngine> fallback_asr;
    if (config_.asr_fallback.enabled && !config_.asr_fallback.app_id.empty()) {
        auto iflytek = std::make_unique<IFlytekCloudASR>();
        if (iflytek->initialize(config_.asr_fallback.app_id,
                                 config_.asr_fallback.api_key,
                                 config_.asr_fallback.api_secret)) {
            spdlog::info("ASR fallback (iFlytek) configured");
            fallback_asr = std::move(iflytek);
        }
    } else {
        spdlog::info("ASR fallback not configured — using local only");
    }

    asr_primary_ = std::make_unique<ASRFallbackEngine>(
        std::move(primary_asr), std::move(fallback_asr));
    asr_primary_->set_result_callback(on_asr_result_static, this);

    // ─── LLM engine (primary: llama.cpp, fallback: OpenAI) ──
    auto llama = std::make_unique<LlamaCppEngine>();
    llama->initialize(config_.llm_primary.model_path,
                      config_.llm_primary.n_ctx,
                      config_.llm_primary.n_threads);

    std::unique_ptr<ILLMEngine> openai;
    if (config_.llm_fallback.enabled && !config_.llm_fallback.api_key.empty()) {
        auto oai = std::make_unique<OpenAIEngine>();
        if (oai->initialize(config_.llm_fallback.api_key,
                            config_.llm_fallback.model,
                            config_.llm_fallback.endpoint_url)) {
            openai = std::move(oai);
        }
    }

    llm_primary_ = std::make_unique<LLMFallbackEngine>(
        std::move(llama), std::move(openai));

    initialized_.store(true);
    spdlog::info("VoiceEngine: initialized (Phase 5: all subsystems ready)");
    return true;
}

void VoiceEngine::shutdown() {
    if (!initialized_.load()) return;

    spdlog::info("VoiceEngine: shutting down...");

    running_.store(false);
    if (continuous_thread_.joinable())
        continuous_thread_.join();

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

    state_machine_.force_state(EngineState::Idle);

    if (config_.mode.mode == EngineMode::Continuous) {
        spdlog::info("VoiceEngine: starting continuous dictation mode");
        running_.store(true);
        continuous_thread_ = std::thread(&VoiceEngine::continuous_loop, this);
    } else {
        spdlog::info("VoiceEngine: started in PTT mode");
    }
}

void VoiceEngine::stop() {
    running_.store(false);

    if (ptt_active_.load()) ptt_release();

    if (continuous_thread_.joinable())
        continuous_thread_.join();

    if (audio_ && audio_->is_running())
        audio_->stop();

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
    drain_audio_and_recognize();
}

// ─── Continuous dictation mode ──────────────────────────

void VoiceEngine::continuous_loop() {
    spdlog::info("Continuous mode: starting audio capture");

    audio_->ring_buffer().reset();
    vad_.reset();

    if (!audio_->start(config_.audio.sample_rate,
                       config_.audio.channels,
                       audio_callback_static, this)) {
        notify_error(-3, "Failed to start continuous audio", "audio", true);
        return;
    }

    // Poll ring buffer and VAD state
    while (running_.load()) {
        auto& rb = audio_->ring_buffer();
        std::size_t available = rb.read_available();

        // Process audio in ~30ms frames
        const int frame_ms = 30;
        std::size_t frame_samples = static_cast<std::size_t>(
            config_.audio.sample_rate * frame_ms / 1000);

        if (available >= frame_samples) {
            std::vector<float> frame(frame_samples);
            rb.read(frame.data(), frame_samples);

            // Compute audio level for observers
            float rms = 0.0f;
            for (auto s : frame) rms += s * s;
            rms = std::sqrt(rms / static_cast<float>(frame_samples));
            float db = 20.0f * std::log10(rms + 1e-10f);
            notify_audio_level(db, db);

            // Run VAD
            EnergyVAD::State vad_state = vad_.process(frame.data(), frame_samples);

            if (vad_state == EnergyVAD::State::Speech) {
                if (!segment_active_.exchange(true)) {
                    // Speech started — begin new segment
                    segment_buffer_.clear();
                    auto prev = state_machine_.state();
                    state_machine_.transition_to(EngineState::Listening);
                    notify_state_change(prev, EngineState::Listening,
                                        "Continuous: speech detected");
                    spdlog::debug("Continuous: speech started");
                }
                // Accumulate samples
                segment_buffer_.insert(segment_buffer_.end(),
                                       frame.begin(), frame.end());
            } else if (segment_active_.load() && !segment_buffer_.empty()) {
                // VAD transitioned from Speech to Silence —
                // but we handle silence timeout inside VAD,
                // so this is the actual end-of-utterance signal
                spdlog::debug("Continuous: speech ended ({} samples, {:.1f}s)",
                              segment_buffer_.size(),
                              static_cast<float>(segment_buffer_.size())
                              / static_cast<float>(config_.audio.sample_rate));
                process_continuous_segment();
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    audio_->stop();
    spdlog::info("Continuous mode: stopped");
}

void VoiceEngine::process_continuous_segment() {
    segment_active_.store(false);

    if (segment_buffer_.empty()) return;

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Recognizing);
    notify_state_change(prev, EngineState::Recognizing,
                        "Continuous: processing segment");

    // Feed segment to ASR
    asr_primary_->process_audio(segment_buffer_.data(), segment_buffer_.size());
    asr_primary_->end_utterance();

    segment_buffer_.clear();
}

void VoiceEngine::audio_callback_static(const float* samples, std::size_t /*frames*/,
                                         int /*sample_rate*/, void* user_data) {
    // This callback is NOT used for continuous mode — we poll the ring buffer directly.
    // The callback is only for PTT mode (passing nullptr disables it).
    (void)samples;
    (void)user_data;
}

void VoiceEngine::on_audio_samples(const float* samples, std::size_t count) {
    // Direct audio sample handler (for future streaming ASR integration)
    (void)samples;
    (void)count;
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

// ─── Pipeline: ASR result → LLM → output ────────────────

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
    notify_state_change(prev, EngineState::Correcting, "ASR complete, starting correction");
    notify_transcription(result.text, false, result.confidence, result.language);

    if (config_.enable_error_correction && llm_primary_ && llm_primary_->is_ready()) {
        run_error_correction(result.text);
    } else {
        run_intent_formatting(result.text);
    }
}

void VoiceEngine::run_error_correction(const std::string& raw_text) {
    const auto& ec_tmpl = prompt_catalog_.error_correction_template();

    LLMRequest req;
    req.intent = IntentType::General;
    req.temperature = config_.llm_primary.temperature;
    req.max_tokens = config_.llm_primary.max_tokens;
    req.messages = {
        {"system", ec_tmpl.system_prompt},
        {"user", raw_text},
    };

    auto resp = llm_primary_->process_streaming(req, [](const std::string&) { return true; });

    std::string corrected = raw_text;
    if (resp.success && !resp.text.empty()) {
        corrected = resp.text;
        spdlog::info("LLM corrected: '{}' → '{}'", raw_text, corrected);
    } else {
        spdlog::warn("LLM correction failed: {}", resp.error_message);
    }

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Formatting);
    notify_state_change(prev, EngineState::Formatting);

    run_intent_formatting(corrected);
}

void VoiceEngine::run_intent_formatting(const std::string& corrected_text) {
    spdlog::info("Running LLM formatting (intent: {})", intent_type_name(current_intent_));

    auto messages = prompt_catalog_.build_messages(
        current_intent_, context_mgr_.history(), corrected_text);

    LLMRequest req;
    req.intent = current_intent_;
    req.temperature = config_.llm_primary.temperature;
    req.max_tokens = config_.llm_primary.max_tokens;
    req.messages = messages;

    auto resp = llm_primary_->process_streaming(req, [](const std::string&) { return true; });

    std::string formatted = corrected_text;
    if (resp.success && !resp.text.empty()) {
        formatted = resp.text;
        spdlog::info("LLM formatted: '{}'", formatted);
    } else {
        spdlog::warn("LLM formatting failed: {}", resp.error_message);
    }

    context_mgr_.add_turn(corrected_text, formatted);

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Outputting);
    notify_state_change(prev, EngineState::Outputting, "LLM complete");

    notify_llm_output(corrected_text, corrected_text, formatted,
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

    auto prev = state_machine_.state();
    state_machine_.transition_to(EngineState::Recognizing);
    notify_state_change(prev, EngineState::Recognizing);

    std::vector<float> chunk(available);
    std::size_t read = rb.read(chunk.data(), available);
    chunk.resize(read);

    spdlog::debug("Drained {} samples ({:.1f}s) from ring buffer",
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

void VoiceEngine::notify_audio_level(float peak_db, float rms_db) {
    std::lock_guard<std::mutex> lock(observers_mutex_);
    AudioLevelEvent ev{peak_db, rms_db};
    for (auto& obs : observers_)
        obs->on_audio_level(ev);
}

} // namespace vim
