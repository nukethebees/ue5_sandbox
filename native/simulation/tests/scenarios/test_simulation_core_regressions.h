#pragma once
#include "../support/simulation_test_support.h"

namespace ml {

enum class ESimulationCoreRegressionScenario : std::uint8_t { FixedTickLifecycle, DamageLifecycle };

void run_worldless_simulation_core_regression(ml::simulation_tests::SimulationFixture const& config,
                                              ESimulationCoreRegressionScenario scenario);
void run_worldless_collision_damage(ml::simulation_tests::SimulationFixture const& config);
}
