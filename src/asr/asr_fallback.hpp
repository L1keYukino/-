#pragma once
#include "src/asr/i_asr_engine.hpp"
#include <memory>
#include <string>

namespace vim {

// Composite ASR engine: tries primary (local), falls back to secondary (cloud).
class ASRFallbackEngine : public IASREngine {
public:
    ASRFallbackEngine(std::unique_ptr<IASREngine> primary,
                      std::unique_ptr<IASREngine> fallback);

    bool initialize(const ASREngineConfig& config) override;
    void process_audio(const float* samples, std::size_t count) override;
    void end_utterance() override;
    void set_result_callback(ASRResultCallback callback, void* user_data) override;
    bool is_ready() const override;
    const char* engine_name() const override;
    void reset() override;

private:
    std::unique_ptr<IASREngine> primary_;
    std::unique_ptr<IASREngine> fallback_;
    IASREngine* active_ = nullptr;  // currently routed engine
    ASRResultCallback user_callback_ = nullptr;
    void* callback_user_data_ = nullptr;

    static void forward_result(const ASRResult& result, void* user_data);
};

inline ASRFallbackEngine::ASRFallbackEngine(std::unique_ptr<IASREngine> primary,
                                             std::unique_ptr<IASREngine> fallback)
    : primary_(std::move(primary)), fallback_(std::move(fallback))
{
}

inline bool ASRFallbackEngine::initialize(const ASREngineConfig& config) {
    bool ok = false;
    if (primary_) ok = primary_->initialize(config);
    if (fallback_) fallback_->initialize(config);
    // Route to primary if available, otherwise fallback
    active_ = (primary_ && primary_->is_ready()) ? primary_.get() : fallback_.get();
    return ok || (fallback_ && fallback_->is_ready());
}

inline void ASRFallbackEngine::process_audio(const float* samples, std::size_t count) {
    // Feed both engines (cloud fallback needs audio too if primary fails late)
    if (primary_) primary_->process_audio(samples, count);
    if (fallback_) fallback_->process_audio(samples, count);
}

inline void ASRFallbackEngine::end_utterance() {
    if (active_) active_->end_utterance();
}

inline void ASRFallbackEngine::set_result_callback(ASRResultCallback callback, void* user_data) {
    user_callback_ = callback;
    callback_user_data_ = user_data;
    if (primary_) primary_->set_result_callback(callback, user_data);
    if (fallback_) fallback_->set_result_callback(callback, user_data);
}

inline bool ASRFallbackEngine::is_ready() const {
    return active_ != nullptr;
}

inline const char* ASRFallbackEngine::engine_name() const {
    return active_ ? active_->engine_name() : "None";
}

inline void ASRFallbackEngine::reset() {
    if (primary_) primary_->reset();
    if (fallback_) fallback_->reset();
    active_ = nullptr;
}

} // namespace vim
