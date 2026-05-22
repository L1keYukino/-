#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <string>
#include <vector>
#include "src/llm/llm_types.hpp"

namespace vim {

using StreamingCallback = std::function<bool(const std::string& token)>;

class ILLMEngine {
public:
    virtual ~ILLMEngine() = default;

    virtual bool initialize(const std::string& model_path,
                            int n_ctx, int n_threads) = 0;

    virtual std::future<LLMResponse> process_async(const LLMRequest& request) = 0;

    virtual LLMResponse process_streaming(const LLMRequest& request,
                                          const StreamingCallback& cb) = 0;

    virtual bool is_ready() const = 0;
    virtual const char* engine_name() const = 0;
    virtual void reset_context() = 0;
};

} // namespace vim
