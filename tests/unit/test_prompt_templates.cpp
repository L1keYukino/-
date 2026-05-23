#include <catch2/catch_test_macros.hpp>
#include "src/llm/prompt_templates.hpp"

namespace vim {

TEST_CASE("PromptCatalog has all intents", "[prompt]") {
    PromptCatalog cat;
    REQUIRE(cat.all().size() == 7);  // +Summary

    for (const auto& t : cat.all()) {
        REQUIRE_FALSE(t.system_prompt.empty());
        REQUIRE_FALSE(t.name.empty());
    }
}

TEST_CASE("PromptCatalog::get returns correct template", "[prompt]") {
    PromptCatalog cat;

    auto& email = cat.get(IntentType::Email);
    REQUIRE(email.intent == IntentType::Email);
    REQUIRE(email.few_shot_examples.size() == 1);

    auto& cmd = cat.get(IntentType::Command);
    REQUIRE(cmd.intent == IntentType::Command);
    REQUIRE_FALSE(cmd.few_shot_examples.empty());
}

TEST_CASE("build_messages produces correct structure", "[prompt]") {
    PromptCatalog cat;
    auto messages = cat.build_messages(IntentType::Chat, {}, "hello world");

    REQUIRE_FALSE(messages.empty());
    REQUIRE(messages[0].role == "system");
    REQUIRE_FALSE(messages[0].content.empty());

    // Last message should be the user input
    REQUIRE(messages.back().role == "user");
    REQUIRE(messages.back().content == "hello world");
}

TEST_CASE("build_messages includes history", "[prompt]") {
    PromptCatalog cat;
    std::vector<LLMMessage> history = {
        {"user", "previous question"},
        {"assistant", "previous answer"},
    };

    auto messages = cat.build_messages(IntentType::General, history, "new text");

    // Check history is preserved before final user message
    REQUIRE(messages[messages.size() - 3].content == "previous question");
    REQUIRE(messages[messages.size() - 2].content == "previous answer");
    REQUIRE(messages.back().content == "new text");
}

TEST_CASE("build_messages includes few-shot examples", "[prompt]") {
    PromptCatalog cat;
    auto messages = cat.build_messages(IntentType::Email, {}, "draft email about meeting");

    // Few-shot: system, user example, assistant example, user input
    bool found_example = false;
    for (const auto& m : messages) {
        if (m.content.find("Subject:") != std::string::npos)
            found_example = true;
    }
    REQUIRE(found_example);
}

TEST_CASE("error_correction_template exists", "[prompt]") {
    PromptCatalog cat;
    auto& ec = cat.error_correction_template();
    REQUIRE_FALSE(ec.system_prompt.empty());
    REQUIRE_FALSE(ec.system_prompt.empty());
}

} // namespace vim
