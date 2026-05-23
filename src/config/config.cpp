#include "src/config/config.hpp"
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace vim {

// ─── Auto-serialized structs (no enums) ────────────────────

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioConfig,
    backend, device_id, sample_rate, channels,
    frame_duration_ms, ring_buffer_size_ms)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ASRPrimaryConfig,
    type, model_dir, enable_vad, vad_silence_timeout_s, vad_speech_timeout_s)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ASRFallbackConfig,
    type, app_id, api_key, api_secret, endpoint_url, enabled)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LLMPrimaryConfig,
    type, model_path, n_ctx, n_threads, n_gpu_layers, temperature, max_tokens)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OutputConfig,
    type, typing_delay_ms, use_unicode_fallback)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HotkeyConfig,
    modifiers, virtual_key)

// ─── ModeConfig (has EngineMode enum) ──────────────────────

void to_json(nlohmann::json& j, const ModeConfig& c) {
    j["type"] = c.mode == EngineMode::PTT ? "ptt" : "continuous";
    j["ptt_hotkey"] = c.ptt_hotkey;
    j["continuous_vad_silence_ms"] = c.continuous_vad_silence_ms;
    j["continuous_max_segment_ms"] = c.continuous_max_segment_ms;
}

void from_json(const nlohmann::json& j, ModeConfig& c) {
    std::string mode_str = j.value("type", "ptt");
    c.mode = (mode_str == "continuous") ? EngineMode::Continuous : EngineMode::PTT;
    if (j.contains("ptt_hotkey")) j.at("ptt_hotkey").get_to(c.ptt_hotkey);
    c.continuous_vad_silence_ms = j.value("continuous_vad_silence_ms", 800);
    c.continuous_max_segment_ms = j.value("continuous_max_segment_ms", 30000);
}

// ─── LLMFallbackConfig (has LLMFallbackType enum) ──────────

void to_json(nlohmann::json& j, const LLMFallbackConfig& c) {
    switch (c.type) {
        case LLMFallbackType::OpenAI: j["type"] = "openai"; break;
        case LLMFallbackType::Claude: j["type"] = "claude"; break;
        default: j["type"] = "none"; break;
    }
    j["api_key"] = c.api_key;
    j["model"] = c.model;
    j["endpoint_url"] = c.endpoint_url;
    j["enabled"] = c.enabled;
}

void from_json(const nlohmann::json& j, LLMFallbackConfig& c) {
    std::string t = j.value("type", "none");
    if (t == "openai") c.type = LLMFallbackType::OpenAI;
    else if (t == "claude") c.type = LLMFallbackType::Claude;
    else c.type = LLMFallbackType::None;
    c.api_key = j.value("api_key", "");
    c.model = j.value("model", "gpt-4o-mini");
    c.endpoint_url = j.value("endpoint_url", "https://api.openai.com/v1");
    c.enabled = j.value("enabled", false);
}

// ─── EngineConfig (top-level, has IntentType enum) ─────────

static IntentType parse_intent(const std::string& s) {
    if (s == "email")          return IntentType::Email;
    if (s == "chat")           return IntentType::Chat;
    if (s == "code_comment")   return IntentType::CodeComment;
    if (s == "documentation")  return IntentType::Documentation;
    if (s == "command")        return IntentType::Command;
    return IntentType::General;
}

void to_json(nlohmann::json& j, const EngineConfig& c) {
    std::vector<std::string> intents;
    for (auto i : c.enabled_intents) intents.push_back(intent_type_name(i));

    j["audio"] = c.audio;
    j["asr_primary"] = c.asr_primary;
    j["asr_fallback"] = c.asr_fallback;
    j["llm_primary"] = c.llm_primary;
    j["llm_fallback"] = c.llm_fallback;
    j["output"] = c.output;
    j["mode"] = c.mode;
    j["default_intent"] = intent_type_name(c.default_intent);
    j["enabled_intents"] = intents;
    j["max_context_turns"] = c.max_context_turns;
    j["enable_error_correction"] = c.enable_error_correction;
}

void from_json(const nlohmann::json& j, EngineConfig& c) {
    j.at("audio").get_to(c.audio);
    j.at("asr_primary").get_to(c.asr_primary);
    j.at("asr_fallback").get_to(c.asr_fallback);
    j.at("llm_primary").get_to(c.llm_primary);
    j.at("llm_fallback").get_to(c.llm_fallback);
    j.at("output").get_to(c.output);
    j.at("mode").get_to(c.mode);
    c.default_intent = parse_intent(j.value("default_intent", "general"));
    c.max_context_turns = j.value("max_context_turns", 5);
    c.enable_error_correction = j.value("enable_error_correction", true);

    c.enabled_intents.clear();
    if (j.contains("enabled_intents")) {
        for (const auto& s : j.at("enabled_intents"))
            c.enabled_intents.push_back(parse_intent(s.get<std::string>()));
    }
    if (c.enabled_intents.empty())
        c.enabled_intents = {IntentType::General};
}

// ─── Public API ─────────────────────────────────────────

EngineConfig load_config_from_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open config file: " + path);
    nlohmann::json j;
    f >> j;
    return j.get<EngineConfig>();
}

void save_config_to_file(const std::string& path, const EngineConfig& cfg) {
    nlohmann::json j = cfg;
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write config file: " + path);
    f << j.dump(2);
}

EngineConfig make_default_config() {
    EngineConfig c;
    return c;
}

std::vector<std::string> validate_config(const EngineConfig& cfg) {
    std::vector<std::string> issues;

    if (cfg.audio.sample_rate != 16000 && cfg.audio.sample_rate != 8000
        && cfg.audio.sample_rate != 44100 && cfg.audio.sample_rate != 48000)
        issues.push_back("audio.sample_rate: expected 8000, 16000, 44100, or 48000");

    if (cfg.audio.channels < 1 || cfg.audio.channels > 8)
        issues.push_back("audio.channels: must be between 1 and 8");

    if (cfg.audio.frame_duration_ms < 10 || cfg.audio.frame_duration_ms > 200)
        issues.push_back("audio.frame_duration_ms: must be between 10 and 200");

    if (cfg.audio.ring_buffer_size_ms < 500 || cfg.audio.ring_buffer_size_ms > 10000)
        issues.push_back("audio.ring_buffer_size_ms: must be between 500 and 10000");

    if (cfg.output.typing_delay_ms < 0 || cfg.output.typing_delay_ms > 500)
        issues.push_back("output.typing_delay_ms: must be between 0 and 500");

    if (cfg.max_context_turns < 1 || cfg.max_context_turns > 50)
        issues.push_back("max_context_turns: must be between 1 and 50");

    if (cfg.enabled_intents.empty())
        issues.push_back("enabled_intents: at least one intent must be enabled");

    if (cfg.mode.continuous_vad_silence_ms < 100
        || cfg.mode.continuous_vad_silence_ms > 5000)
        issues.push_back("mode.continuous_vad_silence_ms: must be between 100 and 5000");

    if (cfg.mode.continuous_max_segment_ms < 1000
        || cfg.mode.continuous_max_segment_ms > 120000)
        issues.push_back("mode.continuous_max_segment_ms: must be between 1000 and 120000");

    return issues;
}

} // namespace vim
