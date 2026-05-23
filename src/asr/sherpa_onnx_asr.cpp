#include "src/asr/sherpa_onnx_asr.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <spdlog/spdlog.h>

#if __has_include("sherpa-onnx/c-api/c-api.h")
  #include "sherpa-onnx/c-api/c-api.h"
  #define VIM_HAS_SHERPA_ONNX 1
#else
  #define VIM_HAS_SHERPA_ONNX 0
#endif

namespace vim {

struct SherpaOnnxASR::Impl {
#if VIM_HAS_SHERPA_ONNX
    const SherpaOnnxOfflineRecognizer* recognizer = nullptr;
    const SherpaOnnxOfflineStream* stream = nullptr;
#endif
};

SherpaOnnxASR::SherpaOnnxASR()
    : impl_(std::make_unique<Impl>())
{
}

SherpaOnnxASR::~SherpaOnnxASR() {
    reset();
}

bool SherpaOnnxASR::initialize(const ASREngineConfig& config) {
    sample_rate_ = config.sample_rate;

#if VIM_HAS_SHERPA_ONNX
    SherpaOnnxOfflineRecognizerConfig recfg;
    std::memset(&recfg, 0, sizeof(recfg));

    std::string model_path = config.model_dir + "/model.onnx"; // full precision, better accuracy
    std::string tokens_path = config.model_dir + "/tokens.txt";

    recfg.model_config.sense_voice.model = model_path.c_str();
    recfg.model_config.sense_voice.use_itn = 1;
    recfg.model_config.tokens = tokens_path.c_str();
    recfg.model_config.num_threads = 2;
    recfg.model_config.debug = 0;
    recfg.model_config.provider = "cpu";

    impl_->recognizer = SherpaOnnxCreateOfflineRecognizer(&recfg);
    if (!impl_->recognizer) {
        spdlog::error("SherpaOnnxASR: failed to create recognizer");
        return false;
    }

    impl_->stream = SherpaOnnxCreateOfflineStream(impl_->recognizer);
    if (!impl_->stream) {
        spdlog::error("SherpaOnnxASR: failed to create stream");
        SherpaOnnxDestroyOfflineRecognizer(impl_->recognizer);
        impl_->recognizer = nullptr;
        return false;
    }

    spdlog::info("SherpaOnnxASR: initialized (model={})", model_path);
    ready_.store(true);
    return true;
#else
    spdlog::warn("SherpaOnnxASR: sherpa-onnx SDK not available — stub mode");
    spdlog::warn("  Expected: {}/model.int8.onnx", config.model_dir);
    ready_.store(true);
    return true;
#endif
}

void SherpaOnnxASR::process_audio(const float* samples, std::size_t count) {
    if (!ready_.load()) return;
    accumulated_samples_.insert(accumulated_samples_.end(), samples, samples + count);
}

void SherpaOnnxASR::end_utterance() {
    spdlog::info("SherpaOnnxASR::end_utterance() called, ready={}, samples={}",
                 ready_.load(), accumulated_samples_.size());
    if (!ready_.load() || accumulated_samples_.empty()) {
        spdlog::warn("SherpaOnnxASR: skipping — ready={}, samples={}",
                     ready_.load(), accumulated_samples_.size());
        return;
    }

#if VIM_HAS_SHERPA_ONNX
    if (!impl_->stream) return;

    auto t0 = std::chrono::steady_clock::now();

    spdlog::info("SherpaOnnxASR: decoding {} samples at {} Hz...",
                 accumulated_samples_.size(), sample_rate_);

    SherpaOnnxAcceptWaveformOffline(
        impl_->stream, sample_rate_,
        accumulated_samples_.data(),
        static_cast<int32_t>(accumulated_samples_.size()));

    SherpaOnnxDecodeOfflineStream(impl_->recognizer, impl_->stream);

    const SherpaOnnxOfflineRecognizerResult* result =
        SherpaOnnxGetOfflineStreamResult(impl_->stream);

    spdlog::info("SherpaOnnxASR: result={}, text={}",
                 (const void*)result, result && result->text ? result->text : "(null)");

    auto t1 = std::chrono::steady_clock::now();
    float elapsed_ms = std::chrono::duration<float, std::milli>(t1 - t0).count();

    if (result && result->text && result_callback_) {
        ASRResult r;
        r.text = result->text;
        r.confidence = 0.95f;
        r.language = result->lang ? result->lang : "zh";
        r.is_partial = false;
        r.timestamp_ms = 0;
        result_callback_(r, callback_user_data_);

        spdlog::info("SherpaOnnxASR: '{}' ({:.1f}ms)", result->text, elapsed_ms);
    }

    if (result) SherpaOnnxDestroyOfflineRecognizerResult(result);

    // Offline stream is single-use — recreate for next utterance
    SherpaOnnxDestroyOfflineStream(impl_->stream);
    impl_->stream = SherpaOnnxCreateOfflineStream(impl_->recognizer);
#endif

#if !VIM_HAS_SHERPA_ONNX
    float duration_s = static_cast<float>(accumulated_samples_.size())
                     / static_cast<float>(sample_rate_);
    if (result_callback_) {
        ASRResult r;
        r.text = "[stub:sherpa-onnx] " + std::to_string(static_cast<int>(duration_s * 1000))
               + "ms audio (" + std::to_string(accumulated_samples_.size()) + " samples)";
        r.confidence = 1.0f;
        r.language = "zh";
        r.is_partial = false;
        r.timestamp_ms = 0;
        result_callback_(r, callback_user_data_);
    }
#endif

    accumulated_samples_.clear();
}

void SherpaOnnxASR::set_result_callback(ASRResultCallback callback, void* user_data) {
    result_callback_ = callback;
    callback_user_data_ = user_data;
}

bool SherpaOnnxASR::is_ready() const {
    return ready_.load();
}

void SherpaOnnxASR::reset() {
    accumulated_samples_.clear();
#if VIM_HAS_SHERPA_ONNX
    if (impl_->stream) {
        SherpaOnnxDestroyOfflineStream(impl_->stream);
        impl_->stream = nullptr;
    }
    if (impl_->recognizer) {
        SherpaOnnxDestroyOfflineRecognizer(impl_->recognizer);
        impl_->recognizer = nullptr;
    }
#endif
    ready_.store(false);
}

} // namespace vim
