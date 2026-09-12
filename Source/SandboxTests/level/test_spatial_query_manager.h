#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
void run_worldless_spatial_query_line_of_sight(FAutomationTestBase& test,
                                               FSoftTestAssertions& checks,
                                               USpaceGameLevelConfig const& config);
}
