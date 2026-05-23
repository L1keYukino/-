#include "src/asr/iflytek_cloud_asr.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <spdlog/spdlog.h>

#ifdef _WIN32
  #include <windows.h>
  #include <winhttp.h>
  #include <wincrypt.h>
  #define VIM_HAS_HTTP 1
#else
  #define VIM_HAS_HTTP 0
#endif

namespace vim {

struct IFlytekCloudASR::Impl {
    std::string app_id;
    std::string api_key;
    std::string api_secret;
    std::string endpoint = "wss://iat-api.xfyun.cn/v2/iat";
};

IFlytekCloudASR::IFlytekCloudASR()
    : impl_(std::make_unique<Impl>())
{
}

IFlytekCloudASR::~IFlytekCloudASR() = default;

bool IFlytekCloudASR::initialize(const std::string& app_id,
                                  const std::string& api_key,
                                  const std::string& api_secret) {
    impl_->app_id = app_id;
    impl_->api_key = api_key;
    impl_->api_secret = api_secret;

    if (app_id.empty() || api_key.empty()) {
        spdlog::warn("iFlytek ASR: credentials not set — engine disabled");
        return false;
    }

    spdlog::info("iFlytek ASR: configured (app_id={})", app_id);
    ready_.store(true);
    return true;
}

bool IFlytekCloudASR::initialize(const ASREngineConfig& /*config*/) {
    // Cloud ASR doesn't need local model config
    // Credentials must be set via the other initialize() overload
    return ready_.load();
}

void IFlytekCloudASR::process_audio(const float* samples, std::size_t count) {
    if (!ready_.load()) return;
    accumulated_samples_.insert(accumulated_samples_.end(), samples, samples + count);
}

void IFlytekCloudASR::end_utterance() {
    if (!ready_.load() || accumulated_samples_.empty()) return;

#if VIM_HAS_HTTP
    // iFlytek WebSocket streaming ASR.
    // This is a simplified synchronous version; full implementation
    // would use WinHTTP WebSocket for real streaming.

    // For now: log the request parameters and return a stub result.
    // Full WebSocket implementation requires:
    //   1. HMAC-SHA256 signature generation
    //   2. WebSocket handshake with iFlytek
    //   3. Streaming audio frames with proper framing
    //   4. Parsing JSON result frames

    float duration_s = static_cast<float>(accumulated_samples_.size())
                     / static_cast<float>(sample_rate_);

    // Convert PCM float → 16-bit PCM (iFlytek expects 16kHz, 16bit, mono)
    std::vector<short> pcm16(accumulated_samples_.size());
    for (std::size_t i = 0; i < accumulated_samples_.size(); ++i) {
        float clamped = std::max(-1.0f, std::min(1.0f, accumulated_samples_[i]));
        pcm16[i] = static_cast<short>(clamped * 32767.0f);
    }

    spdlog::info("iFlytek ASR: sending {:.1f}s of audio ({} samples, {} bytes PCM16)",
                 duration_s, accumulated_samples_.size(), pcm16.size() * 2);

    // Stub response for now (real implementation needs WebSocket)
    if (result_callback_) {
        ASRResult r;
        r.text = "[iFlytek stub] " + std::to_string(static_cast<int>(duration_s * 1000))
               + "ms audio sent to cloud ASR";
        r.confidence = 0.90f;
        r.language = "zh";
        r.is_partial = false;
        r.timestamp_ms = 0;
        result_callback_(r, callback_user_data_);
    }
#else
    if (result_callback_) {
        ASRResult r;
        r.text = "[iFlytek stub: no HTTP] " + std::to_string(accumulated_samples_.size()) + " samples";
        r.confidence = 1.0f;
        r.language = "zh";
        r.is_partial = false;
        r.timestamp_ms = 0;
        result_callback_(r, callback_user_data_);
    }
#endif

    accumulated_samples_.clear();
}

void IFlytekCloudASR::set_result_callback(ASRResultCallback callback, void* user_data) {
    result_callback_ = callback;
    callback_user_data_ = user_data;
}

bool IFlytekCloudASR::is_ready() const {
    return ready_.load();
}

void IFlytekCloudASR::reset() {
    accumulated_samples_.clear();
    ready_.store(false);
}

} // namespace vim
