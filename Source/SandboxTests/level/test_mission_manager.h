#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
enum class EMissionManagerScenario : uint8 {
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

void run_worldless_mission_manager_scenario(FAutomationTestBase& test,
                                            USpaceGameLevelConfig const& config,
                                            EMissionManagerScenario scenario);
}
