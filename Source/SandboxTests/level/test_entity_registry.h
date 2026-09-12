#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
enum class EEntityRegistryScenario : uint8 { TeamCounts, OnePlayerKill, TwoPlayerKills };

void run_worldless_entity_registry_scenario(FAutomationTestBase& test,
                                            FSoftTestAssertions& checks,
                                            USpaceGameLevelConfig const& config,
                                            EEntityRegistryScenario scenario);
}
