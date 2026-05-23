#include "src/llm/llamacpp_engine.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <spdlog/spdlog.h>

#if __has_include("llama.h")
  #include "llama.h"
  #define VIM_HAS_LLAMACPP 1
#else
  #define VIM_HAS_LLAMACPP 0
#endif

namespace vim {

// ─── PIMPL ──────────────────────────────────────────────

struct LlamaCppEngine::Impl {
#if VIM_HAS_LLAMACPP
    llama_model*   model   = nullptr;
    llama_context* ctx     = nullptr;
    const llama_vocab* vocab = nullptr;
#endif
    std::string model_path;
};

LlamaCppEngine::LlamaCppEngine()
    : impl_(std::make_unique<Impl>())
{
}

LlamaCppEngine::~LlamaCppEngine() {
#if VIM_HAS_LLAMACPP
    if (impl_->ctx)   llama_free(impl_->ctx);
    if (impl_->model) llama_model_free(impl_->model);
#endif
}

// ─── Initialize ─────────────────────────────────────────

bool LlamaCppEngine::initialize(const std::string& model_path,
                                 int n_ctx, int n_threads) {
    n_ctx_ = n_ctx;
    n_threads_ = n_threads;
    impl_->model_path = model_path;

#if VIM_HAS_LLAMACPP
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    impl_->model = llama_model_load(model_path.c_str(), mparams);
    if (!impl_->model) {
        spdlog::error("llama.cpp: failed to load model from {}", model_path);
        return false;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = static_cast<uint32_t>(n_ctx);
    cparams.n_threads = n_threads;
    cparams.n_threads_batch = n_threads;
    impl_->ctx = llama_init_from_model(impl_->model, cparams);
    if (!impl_->ctx) {
        spdlog::error("llama.cpp: failed to create context");
        llama_model_free(impl_->model);
        impl_->model = nullptr;
        return false;
    }

    impl_->vocab = llama_model_get_vocab(impl_->model);

    spdlog::info("llama.cpp: loaded model (n_ctx={}, threads={})", n_ctx, n_threads);
    ready_.store(true);
    return true;
#else
    spdlog::warn("llama.cpp: llama.h not found — stub mode");
    spdlog::warn("llama.cpp: model path = {} (not loaded)", model_path);
    ready_.store(true);
    return true;
#endif
}

// ─── Inference ──────────────────────────────────────────

std::future<LLMResponse> LlamaCppEngine::process_async(const LLMRequest& request) {
    return std::async(std::launch::async, [this, request]() {
        return do_inference(request, nullptr);
    });
}

LLMResponse LlamaCppEngine::process_streaming(const LLMRequest& request,
                                               const StreamingCallback& cb) {
    return do_inference(request, &cb);
}

LLMResponse LlamaCppEngine::do_inference(const LLMRequest& request,
                                          const StreamingCallback* cb) {
    LLMResponse resp;
    resp.detected_intent = request.intent;

#if VIM_HAS_LLAMACPP
    if (!ready_.load() || !impl_->ctx) {
        resp.success = false;
        resp.error_message = "llama.cpp: engine not ready";
        return resp;
    }

    // Build the prompt from messages (simple chat format)
    std::ostringstream prompt;
    for (const auto& msg : request.messages) {
        if (msg.role == "system") {
            prompt << "<|system|>\n" << msg.content << "<|end|>\n";
        } else if (msg.role == "user") {
            prompt << "<|user|>\n" << msg.content << "<|end|>\n";
        } else if (msg.role == "assistant") {
            prompt << "<|assistant|>\n" << msg.content << "<|end|>\n";
        }
    }
    prompt << "<|assistant|>\n";

    std::string full_prompt = prompt.str();
    std::vector<llama_token> tokens;
    tokens.reserve(static_cast<std::size_t>(n_ctx_));

    // Tokenize
    int n_tokens = llama_tokenize(impl_->vocab,
                                   full_prompt.c_str(),
                                   static_cast<int>(full_prompt.size()),
                                   tokens.data(),
                                   static_cast<int>(tokens.capacity()),
                                   true, false);
    if (n_tokens < 0) {
        n_tokens = -n_tokens;
        tokens.resize(static_cast<std::size_t>(n_tokens));
        llama_tokenize(impl_->vocab,
                       full_prompt.c_str(),
                       static_cast<int>(full_prompt.size()),
                       tokens.data(),
                       n_tokens, true, false);
    } else {
        tokens.resize(static_cast<std::size_t>(n_tokens));
    }

    // Generate
    std::ostringstream output;
    int n_predict = request.max_tokens;
    for (int i = 0; i < n_predict; ++i) {
        int ret = llama_decode(impl_->ctx, llama_batch_get_one(&tokens.back(), 1));
        if (ret != 0) break;

        llama_token new_token = llama_sampler_sample(llama_sampler_chain_default(), impl_->ctx, -1);

        if (llama_vocab_is_eog(impl_->vocab, new_token)) break;

        char buf[256];
        int len = llama_token_to_piece(impl_->vocab, new_token, buf, sizeof(buf), 0, true);
        if (len > 0) {
            std::string piece(buf, static_cast<std::size_t>(len));
            output << piece;
            if (cb) (*cb)(piece);
        }
        tokens.push_back(new_token);
    }

    resp.text = output.str();
    resp.prompt_tokens = static_cast<int>(tokens.size());
    resp.completion_tokens = static_cast<int>(tokens.size()) - n_tokens;
    resp.success = true;
    return resp;
#else
    // Stub: echo the last user message with a prefix
    (void)cb;
    std::string last_user;
    for (const auto& msg : request.messages)
        if (msg.role == "user") last_user = msg.content;

    resp.text = "[stub:llama.cpp] " + last_user;
    resp.success = true;
    return resp;
#endif
}

bool LlamaCppEngine::is_ready() const {
    return ready_.load();
}

void LlamaCppEngine::reset_context() {
#if VIM_HAS_LLAMACPP
    if (impl_->ctx) {
        llama_kv_cache_clear(impl_->ctx);
    }
#endif
}

} // namespace vim
