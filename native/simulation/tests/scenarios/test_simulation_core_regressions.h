#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {

enum class SimulationCoreRegressionScenario : std::uint8_t { FixedTickLifecycle, DamageLifecycle };

void run_worldless_simulation_core_regression(tests::SimulationFixture const& config,
                                              SimulationCoreRegressionScenario scenario);
void run_worldless_collision_damage(tests::SimulationFixture const& config);
}
