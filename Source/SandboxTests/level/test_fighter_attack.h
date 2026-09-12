#pragma once

#include <CoreMinimal.h>

class FAutomationTestBase;
class USpaceGameLevelConfig;

namespace ml {
struct FSoftTestAssertions;
void run_worldless_fighter_attack(FAutomationTestBase& test,
                                  FSoftTestAssertions& checks,
                                  USpaceGameLevelConfig const& config);
void run_worldless_fighter_obstacle_avoidance(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config);
void run_worldless_fighter_capital_obstruction(FAutomationTestBase& test,
                                               FSoftTestAssertions& checks,
                                               USpaceGameLevelConfig const& config);
void run_worldless_fighter_clear_navigation(FAutomationTestBase& test,
                                            FSoftTestAssertions& checks,
                                            USpaceGameLevelConfig const& config);
void run_worldless_fighter_separation(FAutomationTestBase& test,
                                      FSoftTestAssertions& checks,
                                      USpaceGameLevelConfig const& config);
void run_worldless_fighter_dense_determinism(FAutomationTestBase& test,
                                             FSoftTestAssertions& checks,
                                             USpaceGameLevelConfig const& config);
void run_worldless_fighter_large_cluster(FAutomationTestBase& test,
                                         FSoftTestAssertions& checks,
                                         USpaceGameLevelConfig const& config);
void run_worldless_fighter_hard_avoidance_authority(FAutomationTestBase& test,
                                                    FSoftTestAssertions& checks,
                                                    USpaceGameLevelConfig const& config);
void run_worldless_fighter_navigation_frequency(FAutomationTestBase& test,
                                                FSoftTestAssertions& checks,
                                                USpaceGameLevelConfig const& config);
}
