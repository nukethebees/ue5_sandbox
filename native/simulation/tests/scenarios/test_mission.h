#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class MissionScenario : std::uint8_t {
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
    DefenceObjective,
    RequiredKillsObjective,
    RequiredKillsTimeElapsed,
    AuthoredKillTarget,
    SuccessIsTerminal,
    OutcomeCompletionIsLatched,
};
void run_worldless_mission_scenario(tests::SimulationFixture const& config,
                                    MissionScenario scenario);
}
