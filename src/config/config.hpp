#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "src/core/types.hpp"

namespace vim {

// ─── Per-subsystem config structs ───────────────────────

struct AudioConfig {
    std::string backend = "portaudio";
    std::string device_id;
    int         sample_rate = 16000;
    int         channels = 1;
    int         frame_duration_ms = 30;
    int         ring_buffer_size_ms = 2000;
};

struct ASRPrimaryConfig {
    std::string type = "sherpa_onnx";
    std::string model_dir;
    bool        enable_vad = true;
    float       vad_silence_timeout_s = 0.8f;
    float       vad_speech_timeout_s = 30.0f;
};

struct ASRFallbackConfig {
    std::string type = "iflytek";
    std::string app_id;
    std::string api_key;
    std::string api_secret;
    std::string endpoint_url = "wss://iat-api.xfyun.cn/v2/iat";
    bool        enabled = false;
};

struct LLMPrimaryConfig {
    std::string type = "llama_cpp";
    std::string model_path;
    int         n_ctx = 2048;
    int         n_threads = 4;
    int         n_gpu_layers = 0;
    float       temperature = 0.1f;
    int         max_tokens = 512;
};

struct LLMFallbackConfig {
    LLMFallbackType type = LLMFallbackType::None;
    std::string     api_key;
    std::string     model = "gpt-4o-mini";
    std::string     endpoint_url = "https://api.openai.com/v1";
    bool            enabled = false;
};

struct OutputConfig {
    std::string type = "sendinput";
    int         typing_delay_ms = 10;
    bool        use_unicode_fallback = true;
};

struct HotkeyConfig {
    uint32_t modifiers = 3;       // Ctrl + Alt
    uint32_t virtual_key = 0x56;  // 'V'
};

struct ModeConfig {
    EngineMode  mode = EngineMode::PTT;
    HotkeyConfig ptt_hotkey;
    HotkeyConfig mode_hotkey = {3, 'B'}; // Ctrl+Alt+B
    int         continuous_vad_silence_ms = 800;
    int         continuous_max_segment_ms = 30000;
};

struct EngineConfig {
    AudioConfig           audio;
    ASRPrimaryConfig      asr_primary;
    ASRFallbackConfig     asr_fallback;
    LLMPrimaryConfig      llm_primary;
    LLMFallbackConfig     llm_fallback;
    OutputConfig          output;
    ModeConfig            mode;
    IntentType            default_intent = IntentType::General;
    std::vector<IntentType> enabled_intents = {IntentType::General};
    int                   max_context_turns = 5;
    bool                  enable_error_correction = true;
};

// ─── JSON I/O ───────────────────────────────────────────

/// Load EngineConfig from a JSON file. Throws std::runtime_error on parse failure.
EngineConfig load_config_from_file(const std::string& path);

/// Save current config to JSON.
void save_config_to_file(const std::string& path, const EngineConfig& cfg);

/// Create a default config with sensible defaults.
EngineConfig make_default_config();

/// Validate a config; returns a list of human-readable issues.
/// An empty vector means the config is valid.
std::vector<std::string> validate_config(const EngineConfig& cfg);

} // namespace vim
