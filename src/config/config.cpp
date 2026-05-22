#include "src/config/config.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vim {

// ─── JSON-based loading (stubbed — requires nlohmann/json in Phase 2+) ─

EngineConfig load_config_from_file(const std::string& path) {
    // TODO Phase 2: implement with nlohmann/json
    // For now, return default config with a log warning.
    (void)path;
    return make_default_config();
}

void save_config_to_file(const std::string& path, const EngineConfig& cfg) {
    // TODO Phase 2: implement with nlohmann/json
    (void)path;
    (void)cfg;
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
