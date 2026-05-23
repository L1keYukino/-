#include <catch2/catch_test_macros.hpp>
#include "src/config/config.hpp"
#include <string>

namespace vim {

static std::string cfg_path(const char* rel) {
    return std::string(PROJECT_SOURCE_DIR) + "/" + rel;
}

TEST_CASE("make_default_config produces valid config", "[config]") {
    auto cfg = make_default_config();
    auto issues = validate_config(cfg);
    REQUIRE(issues.empty());
}

TEST_CASE("validate_config catches bad values", "[config]") {
    auto cfg = make_default_config();

    SECTION("bad sample rate") {
        cfg.audio.sample_rate = 12345;
        auto issues = validate_config(cfg);
        REQUIRE_FALSE(issues.empty());
    }

    SECTION("bad channel count") {
        cfg.audio.channels = 0;
        auto issues = validate_config(cfg);
        REQUIRE_FALSE(issues.empty());
    }

    SECTION("bad typing delay") {
        cfg.output.typing_delay_ms = 1000;
        auto issues = validate_config(cfg);
        REQUIRE_FALSE(issues.empty());
    }

    SECTION("bad max context turns") {
        cfg.max_context_turns = 0;
        auto issues = validate_config(cfg);
        REQUIRE_FALSE(issues.empty());
    }

    SECTION("empty enabled intents") {
        cfg.enabled_intents.clear();
        auto issues = validate_config(cfg);
        REQUIRE_FALSE(issues.empty());
    }

    SECTION("bad VAD silence") {
        cfg.mode.continuous_vad_silence_ms = 50;
        auto issues = validate_config(cfg);
        REQUIRE_FALSE(issues.empty());
    }

    SECTION("bad max segment") {
        cfg.mode.continuous_max_segment_ms = 500;
        auto issues = validate_config(cfg);
        REQUIRE_FALSE(issues.empty());
    }
}

TEST_CASE("default_config.json can be parsed", "[config]") {
    auto cfg = load_config_from_file(cfg_path("config/default_config.json").c_str());
    REQUIRE(cfg.audio.backend == "portaudio");
    REQUIRE(cfg.audio.sample_rate == 16000);
    REQUIRE(cfg.audio.channels == 1);
    REQUIRE(cfg.mode.mode == EngineMode::PTT);
    REQUIRE(cfg.default_intent == IntentType::General);
    REQUIRE(cfg.enable_error_correction == true);
    REQUIRE(cfg.max_context_turns == 5);
    REQUIRE_FALSE(cfg.enabled_intents.empty());
}

TEST_CASE("config round-trip through JSON", "[config]") {
    auto cfg = make_default_config();
    cfg.audio.sample_rate = 44100;
    cfg.default_intent = IntentType::Email;

    save_config_to_file(cfg_path("config/test_roundtrip.json").c_str(), cfg);
    auto loaded = load_config_from_file(cfg_path("config/test_roundtrip.json").c_str());

    REQUIRE(loaded.audio.sample_rate == 44100);
    REQUIRE(loaded.default_intent == IntentType::Email);
    REQUIRE(loaded.mode.mode == EngineMode::PTT);
}

TEST_CASE("intent parsing from JSON", "[config]") {
    auto cfg = load_config_from_file(cfg_path("config/default_config.json").c_str());
    REQUIRE(cfg.default_intent == IntentType::General);

    // enabled_intents should include all six
    REQUIRE(cfg.enabled_intents.size() == 6);
}

} // namespace vim
