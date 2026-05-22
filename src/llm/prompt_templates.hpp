#pragma once
#include <string>
#include <utility>
#include <vector>
#include "src/llm/llm_types.hpp"

namespace vim {

struct PromptTemplate {
    IntentType intent;
    std::string name;
    std::string description;
    std::string system_prompt;
    std::vector<std::pair<std::string, std::string>> few_shot_examples;
};

class PromptCatalog {
public:
    PromptCatalog();

    const PromptTemplate& get(IntentType intent) const;
    const std::vector<PromptTemplate>& all() const { return templates_; }

    std::vector<LLMMessage> build_messages(
        IntentType intent,
        const std::vector<LLMMessage>& history,
        const std::string& raw_asr_text) const;

    const PromptTemplate& error_correction_template() const {
        return error_correction_;
    }

private:
    std::vector<PromptTemplate> templates_;
    PromptTemplate error_correction_;
};

} // namespace vim
