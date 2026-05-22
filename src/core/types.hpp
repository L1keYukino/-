#pragma once
#include <cstdint>
#include <string>

namespace vim {

// ─── Engine state ───────────────────────────────────────

enum class EngineState {
    Idle,           // waiting for hotkey or VAD trigger
    Listening,      // audio being captured
    Recognizing,    // ASR is processing
    Correcting,     // LLM error-correction pass
    Formatting,     // LLM intent/formatting pass
    Outputting,     // keystrokes being sent
    Error,          // terminal error — requires reset
};

// ─── Operating mode ─────────────────────────────────────

enum class EngineMode {
    PTT,            // push-to-talk
    Continuous,     // VAD-based continuous dictation
};

// ─── Intent types ───────────────────────────────────────

enum class IntentType {
    General,        // plain dictation + error correction
    Email,          // format as email body
    Chat,           // format as chat message
    CodeComment,    // format as code comment
    Documentation,  // format as formal documentation
    Command,        // format as CLI command
};

// ─── Hotkey modifiers (bitmask) ─────────────────────────

enum class HotkeyMod : uint32_t {
    None  = 0,
    Alt   = 0x0001,
    Ctrl  = 0x0002,
    Shift = 0x0004,
    Win   = 0x0008,
};

inline HotkeyMod operator|(HotkeyMod a, HotkeyMod b) {
    return static_cast<HotkeyMod>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(HotkeyMod a, HotkeyMod b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// ─── LLM fallback type ──────────────────────────────────

enum class LLMFallbackType {
    None,
    OpenAI,
    Claude,
};

// ─── ASR result kind ────────────────────────────────────

enum class ASRResultKind {
    Partial,
    Final,
    Error,
};

// ─── Utility ────────────────────────────────────────────

inline const char* engine_state_name(EngineState s) {
    switch (s) {
        case EngineState::Idle:        return "Idle";
        case EngineState::Listening:   return "Listening";
        case EngineState::Recognizing: return "Recognizing";
        case EngineState::Correcting:  return "Correcting";
        case EngineState::Formatting:  return "Formatting";
        case EngineState::Outputting:  return "Outputting";
        case EngineState::Error:       return "Error";
    }
    return "Unknown";
}

inline const char* intent_type_name(IntentType i) {
    switch (i) {
        case IntentType::General:       return "general";
        case IntentType::Email:          return "email";
        case IntentType::Chat:           return "chat";
        case IntentType::CodeComment:    return "code_comment";
        case IntentType::Documentation:  return "documentation";
        case IntentType::Command:        return "command";
    }
    return "unknown";
}

} // namespace vim
