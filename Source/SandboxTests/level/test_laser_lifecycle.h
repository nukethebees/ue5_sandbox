#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
enum class ELaserLifecycleScenario : uint8 { Hit, SimultaneousLethalHits, Miss, WorldBlocker };

void run_worldless_laser_lifecycle(FAutomationTestBase& test,
                                   FSoftTestAssertions& checks,
                                   USpaceGameLevelConfig const& config,
                                   ELaserLifecycleScenario scenario);
}
