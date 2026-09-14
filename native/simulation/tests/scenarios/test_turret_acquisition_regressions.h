#pragma once
#include "../support/simulation_test_support.h"

namespace ioj::sim {
enum class TurretAcquisitionRegressionScenario : std::uint8_t {
    NoOtherEntity,
    FriendlyOnly,
    EnemyOutsideRadius,
};

void run_worldless_turret_acquisition_regression(ioj::sim::tests::SimulationFixture const& config,
                                                 TurretAcquisitionRegressionScenario scenario);
}
