#include "test_turret_acquisition_regressions.h"

#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <SpaceGame/defences/turrets/TestStaticTurretsConfig.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/defences/turrets/TestStaticTurretsSimulation.h>

#include <SandboxCoreEngine/actor_utils.h>

namespace ml {
void run_worldless_turret_acquisition_regression(
    FAutomationTestBase& test,
    FSoftTestAssertions& checks,
    USpaceGameLevelConfig const& config,
    ETurretAcquisitionRegressionScenario const scenario) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    auto const count{scenario == ETurretAcquisitionRegressionScenario::NoOtherEntity ? 1 : 2};
    data.turret_spawns.add_defaulted(count);
    data.turret_transforms.SetNum(count);
    for (int32 i{}; i < count; ++i) {
        auto const friendly{scenario == ETurretAcquisitionRegressionScenario::FriendlyOnly ||
                            i == 0};
        auto const distance{
            i == 0 ? 0.f
                   : (scenario == ETurretAcquisitionRegressionScenario::EnemyOutsideRadius
                          ? data.turrets.detection_radius + 1.f
                          : 1000.f)};
        data.turret_spawns.locations.set(i, FVector3f{distance, 0.f, 0.f});
        data.turret_spawns.teams[i] = friendly ? ETestTeam::Blue : ETestTeam::Red;
        data.turret_spawns.healths[i] = data.turrets.max_health;
        data.turret_spawns.laser_damages[i] = 0;
        data.turret_transforms[i].SetLocation(FVector{distance, 0.f, 0.f});
    }
    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    harness.timeline.finish_at(0.5);
    test.TestTrue(TEXT("Turret acquisition timeline completes"),
                  harness.run_until_timeline_finished(1.0));
    auto const& turrets{harness.get_simulation().get_turrets()};
    checks.are_equal(count, turrets.get_num_instances(), TEXT("All turrets are registered"));
    for (auto const target : turrets.get_target_handles()) {
        checks.is_true(target.is_null(), TEXT("Invalid candidate does not become a target"));
    }
    checks.are_equal(0,
                     harness.get_simulation().get_lasers().get_number_spawned(),
                     TEXT("Turrets without valid targets do not fire"));
}

}
