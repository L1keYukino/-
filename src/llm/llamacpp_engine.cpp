#include "src/llm/llamacpp_engine.hpp"
#include <algorithm>
#include <chrono>
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
    llama_sampler* smpl    = nullptr;
#endif
    std::string model_path;
};

LlamaCppEngine::LlamaCppEngine()
    : impl_(std::make_unique<Impl>())
{
}

LlamaCppEngine::~LlamaCppEngine() {
#if VIM_HAS_LLAMACPP
    if (impl_->smpl)  llama_sampler_free(impl_->smpl);
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
    ggml_backend_load_all();

    llama_model_params mparams = llama_model_default_params();
    impl_->model = llama_model_load_from_file(model_path.c_str(), mparams);
    if (!impl_->model) {
        spdlog::error("llama.cpp: failed to load model from {}", model_path);
        return false;
    }

    impl_->vocab = llama_model_get_vocab(impl_->model);

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = static_cast<uint32_t>(n_ctx);
    cparams.n_batch = static_cast<uint32_t>(n_ctx);
    cparams.n_threads = n_threads;
    cparams.n_threads_batch = n_threads;

    impl_->ctx = llama_init_from_model(impl_->model, cparams);
    if (!impl_->ctx) {
        spdlog::error("llama.cpp: failed to create context");
        llama_model_free(impl_->model);
        impl_->model = nullptr;
        return false;
    }

    // Set up greedy sampler (low temperature for correction tasks)
    auto sparams = llama_sampler_chain_default_params();
    impl_->smpl = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(impl_->smpl, llama_sampler_init_greedy());

    spdlog::info("llama.cpp: loaded model (n_ctx={}, threads={})", n_ctx, n_threads);
    ready_.store(true);
    return true;
#else
    spdlog::warn("llama.cpp: llama.h not found — stub mode");
    spdlog::warn("  model path = {} (not loaded)", model_path);
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

    // Build ChatML prompt
    std::ostringstream prompt;
    for (const auto& msg : request.messages) {
        if (msg.role == "system")
            prompt << "<|system|>\n" << msg.content << "<|end|>\n";
        else if (msg.role == "user")
            prompt << "<|user|>\n" << msg.content << "<|end|>\n";
        else if (msg.role == "assistant")
            prompt << "<|assistant|>\n" << msg.content << "<|end|>\n";
    }
    prompt << "<|assistant|>\n";

    std::string full_prompt = prompt.str();

    // Tokenize (two-pass: first get count, then fill buffer)
    int n_tokens = -llama_tokenize(impl_->vocab,
                                    full_prompt.c_str(),
                                    static_cast<int>(full_prompt.size()),
                                    nullptr, 0, true, true);
    if (n_tokens <= 0) {
        resp.success = false;
        resp.error_message = "llama.cpp: tokenize failed";
        return resp;
    }

    std::vector<llama_token> tokens(static_cast<std::size_t>(n_tokens));
    if (llama_tokenize(impl_->vocab,
                       full_prompt.c_str(),
                       static_cast<int>(full_prompt.size()),
                       tokens.data(),
                       static_cast<int>(tokens.size()),
                       true, true) < 0) {
        resp.success = false;
        resp.error_message = "llama.cpp: tokenize (2nd pass) failed";
        return resp;
    }

    // Generate tokens
    std::ostringstream output;
    int prompt_token_count = static_cast<int>(tokens.size());
    int n_predict = request.max_tokens;

    llama_batch batch = llama_batch_get_one(tokens.data(),
                                             static_cast<int32_t>(tokens.size()));

    auto t0 = std::chrono::steady_clock::now();
    int n_decoded = 0;

    for (int n_pos = 0;
         n_pos + batch.n_tokens < prompt_token_count + n_predict; ) {

        if (llama_decode(impl_->ctx, batch)) {
            spdlog::warn("llama.cpp: decode failed at pos {}", n_pos);
            break;
        }

        n_pos += batch.n_tokens;

        llama_token new_token = llama_sampler_sample(impl_->smpl, impl_->ctx, -1);

        if (llama_vocab_is_eog(impl_->vocab, new_token)) break;

        char buf[256];
        int len = llama_token_to_piece(impl_->vocab, new_token, buf, sizeof(buf), 0, true);
        if (len > 0) {
            std::string piece(buf, static_cast<std::size_t>(len));
            output << piece;
            if (cb) (*cb)(piece);
        }

        tokens.push_back(new_token);
        n_decoded++;
        batch = llama_batch_get_one(&tokens.back(), 1);
    }

    auto t1 = std::chrono::steady_clock::now();
    float elapsed_s = std::chrono::duration<float>(t1 - t0).count();
    float tps = elapsed_s > 0 ? static_cast<float>(n_decoded) / elapsed_s : 0.0f;

    resp.text = output.str();
    resp.prompt_tokens = prompt_token_count;
    resp.completion_tokens = n_decoded;
    resp.tokens_per_second = tps;
    resp.success = true;

    spdlog::debug("llama.cpp: {} tokens in {:.1f}s ({:.1f} t/s)",
                  n_decoded, elapsed_s, tps);
    return resp;
#else
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
        llama_kv_self_clear(impl_->ctx);
    }
#endif
}

} // namespace vim
