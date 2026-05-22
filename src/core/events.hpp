#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "src/core/types.hpp"

namespace vim {

// ─── Event payloads ─────────────────────────────────────

struct StateChangeEvent {
    EngineState old_state;
    EngineState new_state;
    std::string detail;
};

struct AudioLevelEvent {
    float peak_db;
    float rms_db;
};

struct TranscriptionEvent {
    std::string text;
    bool        is_partial;
    float       confidence;
    std::string language;
};

struct LLMOutputEvent {
    std::string raw_text;
    std::string corrected_text;
    std::string formatted_text;
    IntentType  intent;
    bool        is_streaming;
};

struct ErrorEvent {
    int         code;
    std::string message;
    std::string component;
    bool        is_recoverable;
};

// ─── Observer interface ─────────────────────────────────

class IEngineObserver {
public:
    virtual ~IEngineObserver() = default;

    virtual void on_state_change(const StateChangeEvent& ev) = 0;
    virtual void on_audio_level(const AudioLevelEvent& ev) = 0;
    virtual void on_transcription(const TranscriptionEvent& ev) = 0;
    virtual void on_llm_output(const LLMOutputEvent& ev) = 0;
    virtual void on_error(const ErrorEvent& ev) = 0;
};

using ObserverPtr = std::shared_ptr<IEngineObserver>;

} // namespace vim
