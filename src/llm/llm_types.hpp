#pragma once
#include <string>
#include <vector>
#include "src/core/types.hpp"

namespace vim {

struct LLMMessage {
    std::string role;
    std::string content;
};

struct LLMRequest {
    std::vector<LLMMessage> messages;
    IntentType              intent = IntentType::General;
    float                   temperature = 0.1f;
    int                     max_tokens = 512;
    bool                    stream = false;
};

struct LLMResponse {
    std::string text;
    IntentType  detected_intent = IntentType::General;
    float       tokens_per_second = 0.0f;
    int         prompt_tokens = 0;
    int         completion_tokens = 0;
    bool        success = false;
    std::string error_message;
};

inline LLMResponse llm_error(const std::string& msg) {
    LLMResponse r;
    r.success = false;
    r.error_message = msg;
    return r;
}

} // namespace vim
