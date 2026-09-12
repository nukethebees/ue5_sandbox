#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
void run_worldless_player_ship_vs_capital(FAutomationTestBase& test,
                                          FSoftTestAssertions& checks,
                                          USpaceGameLevelConfig const& config);
}
