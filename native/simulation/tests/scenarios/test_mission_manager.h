#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class MissionManagerScenario : std::uint8_t {
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

void run_worldless_mission_manager_scenario(tests::SimulationFixture const& config,
                                            MissionManagerScenario scenario);
}
