#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
enum class ETurretCombatScenario : uint8 { KillEnemy, ZeroDamage };

void run_worldless_turret_combat(FAutomationTestBase& test,
                                 FSoftTestAssertions& checks,
                                 USpaceGameLevelConfig const& config,
                                 ETurretCombatScenario scenario);
void run_worldless_turret_line_of_sight_blocking(FAutomationTestBase& test,
                                                 FSoftTestAssertions& checks,
                                                 USpaceGameLevelConfig const& config);
void run_worldless_turret_search_requires_line_of_sight(FAutomationTestBase& test,
                                                        FSoftTestAssertions& checks,
                                                        USpaceGameLevelConfig const& config);
}
