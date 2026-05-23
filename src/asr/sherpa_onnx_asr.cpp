#include "src/asr/sherpa_onnx_asr.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>

// sherpa-onnx C API — conditionally included
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
    const SherpaOnnxOfflineModelConfig* model_config = nullptr;
#endif
};

SherpaOnnxASR::SherpaOnnxASR()
    : impl_(std::make_unique<Impl>())
{
}

SherpaOnnxASR::~SherpaOnnxASR() {
#if VIM_HAS_SHERPA_ONNX
    if (impl_->recognizer) {
        SherpaOnnxDestroyOfflineRecognizer(impl_->recognizer);
    }
#endif
}

bool SherpaOnnxASR::initialize(const ASREngineConfig& config) {
    sample_rate_ = config.sample_rate;

#if VIM_HAS_SHERPA_ONNX
    // Configure the offline recognizer with SenseVoice model
    SherpaOnnxOfflineRecognizerConfig recfg;
    std::memset(&recfg, 0, sizeof(recfg));

    SherpaOnnxOfflineModelConfig mcfg;
    std::memset(&mcfg, 0, sizeof(mcfg));
    mcfg.sense_voice.model = (config.model_dir + "/model.int8.onnx").c_str();
    mcfg.sense_voice.tokens = (config.model_dir + "/tokens.txt").c_str();
    mcfg.sense_voice.use_itn = 1;

    // Optional: hotwords file for domain-specific terms
    // mfg.sense_voice.hotwords_file = ...

    recfg.model_config = mcfg;

    impl_->recognizer = SherpaOnnxCreateOfflineRecognizer(&recfg);
    if (!impl_->recognizer) {
        std::fprintf(stderr, "[SherpaOnnxASR] Failed to create recognizer\n");
        return false;
    }

    std::printf("[SherpaOnnxASR] Initialized with model: %s\n",
                config.model_dir.c_str());
    ready_.store(true);
    return true;
#else
    // Stub mode: works without sherpa-onnx linked
    std::fprintf(stderr, "[SherpaOnnxASR] sherpa-onnx not available — stub mode\n");
    std::fprintf(stderr, "[SherpaOnnxASR] Model dir: %s (not loaded)\n",
                 config.model_dir.c_str());
    ready_.store(true);
    return true;
#endif
}

void SherpaOnnxASR::process_audio(const float* samples, std::size_t count) {
    if (!ready_.load()) return;

#if VIM_HAS_SHERPA_ONNX
    accumulated_samples_.insert(accumulated_samples_.end(), samples, samples + count);

    // Try to decode if we have enough samples (>= 0.5 seconds)
    int min_samples = sample_rate_ / 2; // 500ms
    if (static_cast<int>(accumulated_samples_.size()) >= min_samples) {
        // Create a stream from the accumulated buffer
        // sherpa-onnx offline API expects complete audio; for streaming,
        // we'd use the online recognizer API. For now, accumulate and decode.
        // The actual decode happens in end_utterance().
    }
#else
    // Stub: accumulate samples for testing
    accumulated_samples_.insert(accumulated_samples_.end(), samples, samples + count);
#endif
}

void SherpaOnnxASR::end_utterance() {
    if (!ready_.load()) return;

#if VIM_HAS_SHERPA_ONNX
    if (accumulated_samples_.empty() || !impl_->recognizer) return;

    // Create a wave object from accumulated audio
    SherpaOnnxWave wave;
    wave.samples = accumulated_samples_.data();
    wave.sample_rate = sample_rate_;
    wave.num_samples = static_cast<int32_t>(accumulated_samples_.size());

    auto t0 = std::chrono::steady_clock::now();

    const SherpaOnnxOfflineRecognizerResult* result =
        SherpaOnnxGetOfflineRecognizerResult(impl_->recognizer, &wave);

    auto t1 = std::chrono::steady_clock::now();
    float elapsed_ms = std::chrono::duration<float, std::milli>(t1 - t0).count();

    if (result && result->text) {
        ASRResult r;
        r.text = result->text;
        r.confidence = 0.95f; // sherpa-onnx doesn't expose per-result confidence
        r.language = "zh";
        r.is_partial = false;
        r.timestamp_ms = 0;

        if (result_callback_) {
            result_callback_(r, callback_user_data_);
        }

        std::printf("[SherpaOnnxASR] Decoded in %.1fms: '%s'\n",
                    elapsed_ms, result->text);
    }

    SherpaOnnxDestroyOfflineRecognizerResult(result);
#endif

    // Always fire a result in stub mode for testing
#if !VIM_HAS_SHERPA_ONNX
    if (!accumulated_samples_.empty() && result_callback_) {
        float duration_s = static_cast<float>(accumulated_samples_.size()) / static_cast<float>(sample_rate_);
        ASRResult r;
        r.text = "[stub] " + std::to_string(static_cast<int>(duration_s * 1000))
               + "ms of audio (" + std::to_string(accumulated_samples_.size())
               + " samples)";
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
    if (impl_->recognizer) {
        SherpaOnnxDestroyOfflineRecognizer(impl_->recognizer);
        impl_->recognizer = nullptr;
    }
#endif
    ready_.store(false);
}

} // namespace vim
