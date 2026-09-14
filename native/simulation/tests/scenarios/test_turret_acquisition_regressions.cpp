#include "test_turret_acquisition_regressions.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/turrets/sim.h>

namespace ioj::sim {
void run_worldless_turret_acquisition_regression(
    ioj::sim::tests::SimulationFixture const& config,
    TurretAcquisitionRegressionScenario const scenario) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const count{scenario == TurretAcquisitionRegressionScenario::NoOtherEntity ? 1 : 2};
    data.turret_spawns.add_defaulted(count);
    data.turret_transforms.resize(static_cast<std::size_t>(count));
    for (std::int32_t i{}; i < count; ++i) {
        auto const friendly{scenario == TurretAcquisitionRegressionScenario::FriendlyOnly ||
                            i == 0};
        auto const distance{
            i == 0 ? 0.f
                   : (scenario == TurretAcquisitionRegressionScenario::EnemyOutsideRadius
                          ? data.turrets.detection_radius + 1.f
                          : 1000.f)};
        data.turret_spawns.locations.set(i, HMM_V3(distance, 0.f, 0.f));
        data.turret_spawns.teams[i] = friendly ? ioj::sim::Team::Blue : ioj::sim::Team::Red;
        data.turret_spawns.healths[i] = data.turrets.max_health;
        data.turret_spawns.laser_damages[i] = 0;
        data.turret_transforms[i].location = {distance, 0.0, 0.0};
    }
    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    harness.timeline.finish_at(0.5);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(1.0),
                                 "Turret acquisition timeline completes");
    auto const& turrets{harness.get_simulation().get_turrets()};
    ioj::sim::tests::expect_equal(count, turrets.get_num_instances(), "All turrets are registered");
    for (auto const target : turrets.get_target_handles()) {
        ioj::sim::tests::expect_true(target.is_null(),
                                     "Invalid candidate does not become a target");
    }
    ioj::sim::tests::expect_equal(0,
                                  harness.get_simulation().get_lasers().get_number_spawned(),
                                  "Turrets without valid targets do not fire");
}

}
