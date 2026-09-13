#pragma once
#include "../support/simulation_test_support.h"

namespace ml {
enum class ETurretAcquisitionRegressionScenario : std::uint8_t {
    NoOtherEntity,
    FriendlyOnly,
    EnemyOutsideRadius,
};

void run_worldless_turret_acquisition_regression(
    ml::simulation_tests::SimulationFixture const& config,
    ETurretAcquisitionRegressionScenario scenario);
}
