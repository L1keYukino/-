#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace vim {

struct ASRResult {
    std::string text;
    float       confidence = 0.0f;
    int64_t     timestamp_ms = 0;
    std::string language;
    bool        is_partial = true;
};

struct ASREngineConfig {
    std::string model_dir;
    int         sample_rate = 16000;
    bool        enable_vad = true;
    float       vad_silence_timeout_s = 0.8f;
    float       vad_speech_timeout_s = 30.0f;
};

enum class ASREngineResult {
    Success,
    ModelNotFound,
    DeviceError,
    RuntimeError,
};

using ASRResultCallback = void (*)(const ASRResult& result, void* user_data);

class IASREngine {
public:
    virtual ~IASREngine() = default;

    virtual bool initialize(const ASREngineConfig& config) = 0;
    virtual void process_audio(const float* samples, std::size_t count) = 0;
    virtual void end_utterance() = 0;
    virtual void set_result_callback(ASRResultCallback callback, void* user_data) = 0;
    virtual bool is_ready() const = 0;
    virtual const char* engine_name() const = 0;
    virtual void reset() = 0;
};

} // namespace vim
