#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
void run_worldless_fighters_standby_transition(FAutomationTestBase& test,
                                               FSoftTestAssertions& checks,
                                               USpaceGameLevelConfig const& config);
}
