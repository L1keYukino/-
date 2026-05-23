#pragma once
#include "src/asr/i_asr_engine.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace vim {

// iFlytek (科大讯飞) cloud ASR engine.
// Uses WebSocket streaming API (wss://iat-api.xfyun.cn/v2/iat).
// Requires: app_id, api_key, api_secret from https://console.xfyun.cn
class IFlytekCloudASR : public IASREngine {
public:
    IFlytekCloudASR();
    ~IFlytekCloudASR() override;

    bool initialize(const std::string& app_id, const std::string& api_key,
                    const std::string& api_secret);
    // IASREngine interface
    bool initialize(const ASREngineConfig& config) override;
    void process_audio(const float* samples, std::size_t count) override;
    void end_utterance() override;
    void set_result_callback(ASRResultCallback callback, void* user_data) override;
    bool is_ready() const override;
    const char* engine_name() const override { return "iFlytek Cloud"; }
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
