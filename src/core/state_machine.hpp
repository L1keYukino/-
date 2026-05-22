#pragma once
#include <atomic>
#include <string>

namespace vim {

enum class EngineState;

// Simple state machine with transition validation.
// All methods are thread-safe.
class EngineStateMachine {
public:
    EngineStateMachine();

    EngineState state() const;
    bool transition_to(EngineState target);
    void force_state(EngineState s);
    bool is_terminal() const;

private:
    static bool is_valid_transition(EngineState from, EngineState to);

    std::atomic<int> state_;
};

} // namespace vim
