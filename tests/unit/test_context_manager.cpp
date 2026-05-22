#include <catch2/catch_test_macros.hpp>
#include "src/llm/context_manager.hpp"

namespace vim {

TEST_CASE("ContextManager starts empty", "[context]") {
    ContextManager cm(5);
    REQUIRE(cm.history().empty());
    REQUIRE(cm.turn_count() == 0);
}

TEST_CASE("ContextManager adds turns", "[context]") {
    ContextManager cm(5);
    cm.add_turn("hello", "hi there");
    REQUIRE(cm.turn_count() == 1);
    REQUIRE(cm.history().size() == 2);
    REQUIRE(cm.history()[0].role == "user");
    REQUIRE(cm.history()[0].content == "hello");
    REQUIRE(cm.history()[1].role == "assistant");
    REQUIRE(cm.history()[1].content == "hi there");
}

TEST_CASE("ContextManager prunes old turns", "[context]") {
    ContextManager cm(3); // max 3 turns

    cm.add_turn("1", "a");
    cm.add_turn("2", "b");
    cm.add_turn("3", "c");
    cm.add_turn("4", "d"); // should prune turn "1"

    REQUIRE(cm.turn_count() == 3);
    REQUIRE(cm.history()[0].content == "2"); // turn 1 was pruned
}

TEST_CASE("ContextManager clears", "[context]") {
    ContextManager cm(10);
    cm.add_turn("a", "b");
    cm.add_turn("c", "d");
    cm.clear();
    REQUIRE(cm.history().empty());
    REQUIRE(cm.turn_count() == 0);
}

TEST_CASE("ContextManager token-based pruning", "[context]") {
    ContextManager cm(100, 50); // max 50 tokens (~150 chars)

    cm.add_turn(std::string(200, 'x'), std::string(200, 'y'));
    // Should have pruned everything or be under limit
    REQUIRE(cm.estimated_tokens() <= 50);
}

TEST_CASE("ContextManager add_user and add_assistant", "[context]") {
    ContextManager cm(5);
    cm.add_user("user message");
    cm.add_assistant("assistant message");
    REQUIRE(cm.turn_count() == 1);
    REQUIRE(cm.history().size() == 2);
}

TEST_CASE("ContextManager set_max_turns", "[context]") {
    ContextManager cm(10);
    cm.add_turn("1", "a");
    cm.add_turn("2", "b");
    cm.add_turn("3", "c");
    cm.add_turn("4", "d");

    cm.set_max_turns(2);
    REQUIRE(cm.turn_count() == 2);
}

} // namespace vim
