#include "src/engine_api.h"
#include "src/core/engine.hpp"
#include "src/audio/i_audio_capture.hpp"
#include <cstring>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

struct VimEngine {
    vim::EngineConfig config;
    std::unique_ptr<vim::VoiceEngine> engine;
    std::shared_ptr<vim::IEngineObserver> observer;

    vim_state_cb state_cb = nullptr;
    vim_asr_cb   asr_cb   = nullptr;
    vim_llm_cb   llm_cb   = nullptr;
    vim_error_cb error_cb = nullptr;
    vim_audio_cb audio_cb = nullptr;

    std::string last_formatted;
};

class CppObserver : public vim::IEngineObserver {
public:
    VimEngine* owner;
    void on_state_change(const vim::StateChangeEvent& ev) override {
        if (owner->state_cb)
            owner->state_cb(static_cast<int>(ev.old_state), static_cast<int>(ev.new_state), ev.detail.c_str());
    }
    void on_transcription(const vim::TranscriptionEvent& ev) override {
        if (owner->asr_cb)
            owner->asr_cb(ev.text.c_str(), ev.is_partial ? 1 : 0, ev.confidence);
    }
    void on_llm_output(const vim::LLMOutputEvent& ev) override {
        owner->last_formatted = ev.formatted_text;
        spdlog::info("CppObserver::on_llm_output: '{}' ({} bytes)", ev.formatted_text, ev.formatted_text.size());
        if (owner->llm_cb) owner->llm_cb(ev.formatted_text.c_str());
    }
    void on_error(const vim::ErrorEvent& ev) override {
        if (owner->error_cb) owner->error_cb(ev.code, ev.message.c_str());
    }
    void on_audio_level(const vim::AudioLevelEvent& ev) override {
        if (owner->audio_cb) owner->audio_cb(ev.peak_db);
    }
};

extern "C" {

VimEngine* vim_create(const char* config_path) {
    auto* e = new VimEngine();

    // Logger to file
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("vim.log", true);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("vim", sink));
    spdlog::set_level(spdlog::level::info);

    try {
        if (config_path && config_path[0])
            e->config = vim::load_config_from_file(config_path);
        else
            e->config = vim::make_default_config();
    } catch (...) {
        e->config = vim::make_default_config();
    }

    e->engine = std::make_unique<vim::VoiceEngine>(e->config);
    e->engine->initialize();

    auto obs = std::make_shared<CppObserver>();
    obs->owner = e;
    e->observer = obs;
    e->engine->add_observer(obs);

    return e;
}

void vim_destroy(VimEngine* e) {
    if (!e) return;
    e->engine->stop();
    e->engine->shutdown();
    delete e;
}

void vim_start(VimEngine* e)  { if (e) e->engine->start(); }
void vim_stop(VimEngine* e)   { if (e) e->engine->stop(); }
void vim_begin_record(VimEngine* e) { if (e) e->engine->ptt_press(); }
void vim_end_record(VimEngine* e)   { if (e) e->engine->ptt_release(); }

void vim_set_state_callback(VimEngine* e, vim_state_cb cb) { if (e) e->state_cb = cb; }
void vim_set_asr_callback(VimEngine* e, vim_asr_cb cb)     { if (e) e->asr_cb = cb; }
void vim_set_llm_callback(VimEngine* e, vim_llm_cb cb)     { if (e) e->llm_cb = cb; }
void vim_set_error_callback(VimEngine* e, vim_error_cb cb) { if (e) e->error_cb = cb; }
void vim_set_audio_callback(VimEngine* e, vim_audio_cb cb) { if (e) e->audio_cb = cb; }

int vim_is_recording(VimEngine* e)  { return e && e->engine->state() == vim::EngineState::Listening ? 1 : 0; }
int vim_is_processing(VimEngine* e) {
    if (!e) return 0;
    auto s = e->engine->state();
    return (s != vim::EngineState::Idle && s != vim::EngineState::Listening) ? 1 : 0;
}
float vim_audio_peak(VimEngine* e) {
    if (!e) return -60.0f;
    auto* audio = e->engine->audio_capture();
    return audio ? audio->peak_db() : -60.0f;
}

int vim_get_output(VimEngine* e, char* buf, int buf_size) {
    if (!e || !buf || buf_size <= 0) return 0;
    if (e->last_formatted.empty()) return 0;
    int n = std::min(buf_size - 1, static_cast<int>(e->last_formatted.size()));
    std::memcpy(buf, e->last_formatted.c_str(), n);
    buf[n] = '\0';
    spdlog::info("vim_get_output: returning {} bytes", n);
    e->last_formatted.clear();
    return n;
}

int vim_poll_events(VimEngine*) {
    // Stub for hotkey polling — Python handles hotkeys via pynput
    return 1;
}

} // extern "C"
