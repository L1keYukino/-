#pragma once
#include <deque>
#include <string>
#include <vector>
#include "src/llm/llm_types.hpp"

namespace vim {

class ContextManager {
public:
    explicit ContextManager(int max_turns = 5, int max_tokens = 2048)
        : max_turns_(max_turns), max_tokens_(max_tokens) {}

    void add_turn(const std::string& user_text, const std::string& assistant_text) {
        history_.push_back({"user", user_text});
        history_.push_back({"assistant", assistant_text});
        prune();
    }

    void add_user(const std::string& text) {
        history_.push_back({"user", text});
        prune();
    }

    void add_assistant(const std::string& text) {
        history_.push_back({"assistant", text});
        prune();
    }

    const std::vector<LLMMessage>& history() const { return history_; }

    void clear() { history_.clear(); }

    void set_max_turns(int n) { max_turns_ = n; prune(); }
    void set_max_tokens(int n) { max_tokens_ = n; prune(); }

    int turn_count() const {
        int turns = 0;
        for (const auto& m : history_)
            if (m.role == "user") ++turns;
        return turns;
    }

    int estimated_tokens() const {
        int total = 0;
        for (const auto& m : history_)
            total += static_cast<int>(m.content.size()) / 3;
        return total;
    }

private:
    void prune() {
        // Remove oldest turns until within limits
        while (turn_count() > max_turns_ && history_.size() >= 2) {
            // Remove oldest user+assistant pair
            history_.erase(history_.begin());
            history_.erase(history_.begin());
        }
        // Token-based pruning: rough estimate (4 chars ≈ 1 token)
        while (estimated_tokens() > max_tokens_ && history_.size() >= 2) {
            history_.erase(history_.begin());
            history_.erase(history_.begin());
        }
    }

    std::vector<LLMMessage> history_;
    int max_turns_;
    int max_tokens_;
};

} // namespace vim
