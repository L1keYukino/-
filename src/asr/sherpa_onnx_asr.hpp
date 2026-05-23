#pragma once
#include "src/asr/i_asr_engine.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace vim {

// Sherpa-ONNX offline recognizer wrapper.
// Uses the sherpa-onnx C API (sherpa-onnx/c-api/c-api.h).
// Model: SenseVoice (Chinese-optimized, ~100MB).
class SherpaOnnxASR : public IASREngine {
public:
    SherpaOnnxASR();
    ~SherpaOnnxASR() override;

    bool initialize(const ASREngineConfig& config) override;
    void process_audio(const float* samples, std::size_t count) override;
    void end_utterance() override;
    void set_result_callback(ASRResultCallback callback, void* user_data) override;
    bool is_ready() const override;
    const char* engine_name() const override { return "Sherpa-ONNX"; }
    void reset() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ASRResultCallback result_callback_ = nullptr;
    void* callback_user_data_ = nullptr;
    std::atomic<bool> ready_{false};
    std::vector<float> accumulated_samples_;
    int sample_rate_ = 16000;
};

} // namespace vim
