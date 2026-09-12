#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;

enum class ESimulationCoreRegressionScenario : uint8 { FixedTickLifecycle, DamageLifecycle };

void run_worldless_simulation_core_regression(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config,
                                              ESimulationCoreRegressionScenario scenario);
void run_worldless_collision_damage(FAutomationTestBase& test,
                                    FSoftTestAssertions& checks,
                                    USpaceGameLevelConfig const& config);
}
