#pragma once
#include "src/llm/i_llm_engine.hpp"
#include <memory>
#include <string>

namespace vim {

// Composite LLM engine: tries primary (local), falls back to secondary (cloud).
class LLMFallbackEngine : public ILLMEngine {
public:
    LLMFallbackEngine(std::unique_ptr<ILLMEngine> primary,
                      std::unique_ptr<ILLMEngine> fallback);

    bool initialize(const std::string&, int, int) override;
    std::future<LLMResponse> process_async(const LLMRequest& request) override;
    LLMResponse process_streaming(const LLMRequest& request,
                                  const StreamingCallback& cb) override;
    bool is_ready() const override;
    const char* engine_name() const override;
    void reset_context() override;

private:
    std::unique_ptr<ILLMEngine> primary_;
    std::unique_ptr<ILLMEngine> fallback_;
};

inline LLMFallbackEngine::LLMFallbackEngine(std::unique_ptr<ILLMEngine> primary,
                                             std::unique_ptr<ILLMEngine> fallback)
    : primary_(std::move(primary)), fallback_(std::move(fallback))
{
}

inline bool LLMFallbackEngine::initialize(const std::string&, int, int) {
    // Primary and fallback are initialized separately by the caller
    return is_ready();
}

inline std::future<LLMResponse> LLMFallbackEngine::process_async(const LLMRequest& request) {
    if (primary_ && primary_->is_ready()) {
        auto fut = primary_->process_async(request);
        return fut; // could add timeout + fallback logic here
    }
    if (fallback_ && fallback_->is_ready())
        return fallback_->process_async(request);

    std::promise<LLMResponse> p;
    p.set_value(llm_error("No LLM engine available"));
    return p.get_future();
}

inline LLMResponse LLMFallbackEngine::process_streaming(const LLMRequest& request,
                                                         const StreamingCallback& cb) {
    if (primary_ && primary_->is_ready())
        return primary_->process_streaming(request, cb);
    if (fallback_ && fallback_->is_ready())
        return fallback_->process_streaming(request, cb);
    return llm_error("No LLM engine available");
}

inline bool LLMFallbackEngine::is_ready() const {
    return (primary_ && primary_->is_ready()) || (fallback_ && fallback_->is_ready());
}

inline const char* LLMFallbackEngine::engine_name() const {
    if (primary_ && primary_->is_ready()) return primary_->engine_name();
    if (fallback_ && fallback_->is_ready()) return fallback_->engine_name();
    return "None";
}

inline void LLMFallbackEngine::reset_context() {
    if (primary_) primary_->reset_context();
    if (fallback_) fallback_->reset_context();
}

} // namespace vim
