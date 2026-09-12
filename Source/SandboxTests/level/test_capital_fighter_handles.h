#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;

enum class ECapitalFighterHandlesScenario : uint8 { KillFightersOnly, KillCapital, All };

void run_worldless_simultaneous_capital_reassignment(FAutomationTestBase& test,
                                                     FSoftTestAssertions& checks,
                                                     USpaceGameLevelConfig const& config);
void run_worldless_capital_fighter_handles(FAutomationTestBase& test,
                                           FSoftTestAssertions& checks,
                                           USpaceGameLevelConfig const& config,
                                           ECapitalFighterHandlesScenario scenario);
}
