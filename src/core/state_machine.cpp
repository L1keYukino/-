#include "src/core/state_machine.hpp"
#include "src/core/types.hpp"
#include <cassert>

namespace vim {

EngineStateMachine::EngineStateMachine() {
    state_ = static_cast<int>(EngineState::Idle);
}

EngineState EngineStateMachine::state() const {
    return static_cast<EngineState>(state_.load(std::memory_order_acquire));
}

bool EngineStateMachine::transition_to(EngineState target) {
    EngineState expected = state();
    while (true) {
        if (!is_valid_transition(expected, target))
            return false;
        int exp_int = static_cast<int>(expected);
        int tgt_int = static_cast<int>(target);
        if (state_.compare_exchange_weak(exp_int, tgt_int,
                std::memory_order_acq_rel, std::memory_order_acquire))
            return true;
        expected = static_cast<EngineState>(exp_int);
    }
}

void EngineStateMachine::force_state(EngineState s) {
    state_.store(static_cast<int>(s), std::memory_order_release);
}

bool EngineStateMachine::is_terminal() const {
    return state() == EngineState::Error;
}

bool EngineStateMachine::is_valid_transition(EngineState from, EngineState to) {
    // Error is a terminal state — only force_state can escape it
    if (from == EngineState::Error) return false;

    // PTT mode linear flow
    if (from == EngineState::Idle) {
        return to == EngineState::Listening || to == EngineState::Error;
    }
    if (from == EngineState::Listening) {
        return to == EngineState::Recognizing || to == EngineState::Idle || to == EngineState::Error;
    }
    if (from == EngineState::Recognizing) {
        return to == EngineState::Correcting || to == EngineState::Idle || to == EngineState::Error;
    }
    if (from == EngineState::Correcting) {
        return to == EngineState::Formatting || to == EngineState::Idle || to == EngineState::Error;
    }
    if (from == EngineState::Formatting) {
        return to == EngineState::Outputting || to == EngineState::Idle || to == EngineState::Error;
    }
    if (from == EngineState::Outputting) {
        return to == EngineState::Idle || to == EngineState::Error;
    }

    return false;
}

} // namespace vim
