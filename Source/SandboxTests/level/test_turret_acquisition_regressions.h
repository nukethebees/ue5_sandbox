#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
enum class ETurretAcquisitionRegressionScenario : uint8 {
    NoOtherEntity,
    FriendlyOnly,
    EnemyOutsideRadius,
};

void run_worldless_turret_acquisition_regression(FAutomationTestBase& test,
                                                 FSoftTestAssertions& checks,
                                                 USpaceGameLevelConfig const& config,
                                                 ETurretAcquisitionRegressionScenario scenario);
}
