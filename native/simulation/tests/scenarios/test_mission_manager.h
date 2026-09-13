#pragma once
#include "../support/simulation_test_support.h"

namespace ml {
enum class EMissionManagerScenario : std::uint8_t {
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
    DefenceObjective,
    RequiredKillsObjective,
    RequiredKillsTimeElapsed,
    AutomaticKillTarget,
    SuccessIsTerminal,
    ExplicitCompletionIsLatched,
};

void run_worldless_mission_manager_scenario(ml::simulation_tests::SimulationFixture const& config,
                                            EMissionManagerScenario scenario);
}
