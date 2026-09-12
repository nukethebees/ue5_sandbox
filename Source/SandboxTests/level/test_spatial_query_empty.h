#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
void run_worldless_spatial_query_empty(FAutomationTestBase& test,
                                       FSoftTestAssertions& checks,
                                       USpaceGameLevelConfig const& config);
void run_worldless_spatial_query_range(FAutomationTestBase& test,
                                       FSoftTestAssertions& checks,
                                       USpaceGameLevelConfig const& config);
}
