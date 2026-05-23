#pragma once
#include "src/llm/i_llm_engine.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace vim {

// llama.cpp GGUF model inference engine.
// Wraps the llama.h C API for local LLM inference.
// Model: Qwen2.5-1.5B/3B GGUF (Chinese-optimized).
class LlamaCppEngine : public ILLMEngine {
public:
    LlamaCppEngine();
    ~LlamaCppEngine() override;

    bool initialize(const std::string& model_path,
                    int n_ctx, int n_threads) override;
    std::future<LLMResponse> process_async(const LLMRequest& request) override;
    LLMResponse process_streaming(const LLMRequest& request,
                                  const StreamingCallback& cb) override;
    bool is_ready() const override;
    const char* engine_name() const override { return "llama.cpp"; }
    void reset_context() override;

private:
    LLMResponse do_inference(const LLMRequest& request, const StreamingCallback* cb);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> ready_{false};
    int n_ctx_ = 2048;
    int n_threads_ = 4;
};

} // namespace vim
