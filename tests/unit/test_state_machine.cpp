#include <catch2/catch_test_macros.hpp>
#include "src/core/state_machine.hpp"
#include "src/core/types.hpp"

namespace vim {

TEST_CASE("StateMachine starts at Idle", "[state_machine]") {
    EngineStateMachine sm;
    REQUIRE(sm.state() == EngineState::Idle);
    REQUIRE_FALSE(sm.is_terminal());
}

TEST_CASE("StateMachine: valid PTT transitions", "[state_machine]") {
    EngineStateMachine sm;

    SECTION("Idle -> Listening") {
        REQUIRE(sm.transition_to(EngineState::Listening));
        REQUIRE(sm.state() == EngineState::Listening);
    }

    SECTION("Full PTT flow") {
        REQUIRE(sm.transition_to(EngineState::Listening));
        REQUIRE(sm.transition_to(EngineState::Recognizing));
        REQUIRE(sm.transition_to(EngineState::Correcting));
        REQUIRE(sm.transition_to(EngineState::Formatting));
        REQUIRE(sm.transition_to(EngineState::Outputting));
        REQUIRE(sm.transition_to(EngineState::Idle));
    }

    SECTION("Return to Idle from any state") {
        sm.transition_to(EngineState::Listening);
        REQUIRE(sm.transition_to(EngineState::Idle));
        REQUIRE(sm.state() == EngineState::Idle);
    }
}

TEST_CASE("StateMachine rejects invalid transitions", "[state_machine]") {
    EngineStateMachine sm;

    SECTION("Idle -> Recognizing is invalid (skip Listening)") {
        REQUIRE_FALSE(sm.transition_to(EngineState::Recognizing));
        REQUIRE(sm.state() == EngineState::Idle);
    }

    SECTION("Idle -> Correcting is invalid") {
        REQUIRE_FALSE(sm.transition_to(EngineState::Correcting));
    }

    SECTION("Idle -> Formatting is invalid") {
        REQUIRE_FALSE(sm.transition_to(EngineState::Formatting));
    }

    SECTION("Listening -> Formatting is invalid") {
        sm.transition_to(EngineState::Listening);
        REQUIRE_FALSE(sm.transition_to(EngineState::Formatting));
    }
}

TEST_CASE("StateMachine: Error is terminal", "[state_machine]") {
    EngineStateMachine sm;
    sm.force_state(EngineState::Error);
    REQUIRE(sm.is_terminal());
    REQUIRE_FALSE(sm.transition_to(EngineState::Idle));
    REQUIRE(sm.state() == EngineState::Error);
}

TEST_CASE("StateMachine: force_state bypasses validation", "[state_machine]") {
    EngineStateMachine sm;
    sm.force_state(EngineState::Error);
    REQUIRE(sm.state() == EngineState::Error);
    sm.force_state(EngineState::Idle);
    REQUIRE(sm.state() == EngineState::Idle);
}

} // namespace vim
