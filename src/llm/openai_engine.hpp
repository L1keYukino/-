#pragma once
#include "src/llm/i_llm_engine.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace vim {

// OpenAI/Claude-compatible HTTP API engine.
// Uses WinHTTP on Windows (built-in, no extra deps).
class OpenAIEngine : public ILLMEngine {
public:
    OpenAIEngine();
    ~OpenAIEngine() override;

    bool initialize(const std::string& api_key, const std::string& model,
                    const std::string& endpoint_url);
    // ILLMEngine interface override (model_path is repurposed as config JSON or empty)
    bool initialize(const std::string&, int, int) override {
        return false; // use the config-based overload above
    }

    std::future<LLMResponse> process_async(const LLMRequest& request) override;
    LLMResponse process_streaming(const LLMRequest& request,
                                  const StreamingCallback& cb) override;
    bool is_ready() const override;
    const char* engine_name() const override { return "OpenAI"; }
    void reset_context() override {}

    LLMResponse send_http_request(const std::string& body, const StreamingCallback* cb);

private:

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string api_key_;
    std::string model_;
    std::string endpoint_url_;
    std::atomic<bool> ready_{false};
};

} // namespace vim
