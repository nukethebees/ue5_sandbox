#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
void run_worldless_fighter_los_failure(FAutomationTestBase& test,
                                       FSoftTestAssertions& checks,
                                       USpaceGameLevelConfig const& config);
}
