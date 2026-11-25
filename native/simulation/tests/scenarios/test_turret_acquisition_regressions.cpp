#include "../support/simulation_test_support.h"
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/turrets/sim.h>

#include <memory_resource>

namespace ioj::sim {
enum class TurretAcquisitionRegressionScenario : std::uint8_t {
    NoOtherEntity,
    FriendlyOnly,
    EnemyOutsideRadius,
};

void run_worldless_turret_acquisition_regression(
    tests::SimulationFixture const& config, TurretAcquisitionRegressionScenario const scenario) {
    tests::SimulationScenario data{tests::make_simulation_data(config)};
    auto const count{scenario == TurretAcquisitionRegressionScenario::NoOtherEntity ? 1 : 2};
    for (std::int32_t i{}; i < count; ++i) {
        auto const friendly{scenario == TurretAcquisitionRegressionScenario::FriendlyOnly ||
                            i == 0};
        auto const distance{
            i == 0 ? 0.f
                   : (scenario == TurretAcquisitionRegressionScenario::EnemyOutsideRadius
                          ? data.runtime.turrets.detection_radius + 1.f
                          : 1000.f)};
        tests::add_turret_spawn(data,
                                Vector3f{distance, 0.f, 0.f},
                                {},
                                friendly ? Team::Blue : Team::Red,
                                {},
                                ioj::NonNegative<std::int32_t>::make(0));
    }

    LevelSim simulation{std::move(data).compile(), *std::pmr::new_delete_resource()};
    simulation.start();
    simulation.advance(0.5);

    auto const& turrets{simulation.turrets()};
    EXPECT_EQ(count, turrets.num_instances()) << "All turrets are registered";
    for (auto const target : turrets.get_entities().target_ids()) {
        EXPECT_TRUE(!target.is_valid()) << "Invalid candidate does not become a target";
    }

    EXPECT_EQ(0, simulation.lasers().get_number_spawned())
        << "Turrets without valid targets do not fire";
}

TEST(WorldlessSpaceGameSimulation, TurretsNoOtherEntityRemainsIdle) {
    run_worldless_turret_acquisition_regression(tests::make_fixture(),
                                                TurretAcquisitionRegressionScenario::NoOtherEntity);
}

TEST(WorldlessSpaceGameSimulation, TurretsFriendlyOnlyRemainsIdle) {
    run_worldless_turret_acquisition_regression(tests::make_fixture(),
                                                TurretAcquisitionRegressionScenario::FriendlyOnly);
}

TEST(WorldlessSpaceGameSimulation, TurretsEnemyOutsideDetectionRadiusRemainsIdle) {
    run_worldless_turret_acquisition_regression(
        tests::make_fixture(), TurretAcquisitionRegressionScenario::EnemyOutsideRadius);
}

}
