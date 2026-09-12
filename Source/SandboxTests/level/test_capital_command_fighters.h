#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
void run_worldless_capital_command_fighters(FAutomationTestBase& test,
                                            FSoftTestAssertions& checks,
                                            USpaceGameLevelConfig const& config);
}
