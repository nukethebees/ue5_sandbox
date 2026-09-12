#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>

#include <SandboxTests/support/level_checks.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>
#include "test_fighters_standby_transition.h"

#include <SandboxCore/time_series_data.h>

#include <Engine/World.h>
#include <Misc/Optional.h>

namespace ml {
namespace fighters_standby_test {
inline constexpr int32 collision_resilient_health{1'000'000};
}

void run_worldless_fighters_standby_transition(FAutomationTestBase& test,
                                               FSoftTestAssertions& checks,
                                               USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.health = fighters_standby_test::collision_resilient_health;
    add_worldless_capital_spawn(data,
                                FVector3f{-4000.f, 0.f, 0.f},
                                ETestTeam::Green,
                                1,
                                0.f,
                                60.f,
                                fighters_standby_test::collision_resilient_health);
    add_worldless_capital_spawn(data,
                                FVector3f{4000.f, 0.f, 0.f},
                                ETestTeam::Red,
                                0,
                                0.f,
                                60.f,
                                fighters_standby_test::collision_resilient_health);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const enemy{capitals.get_handle(1)};

    struct Sample {
        int32 capital_count{};
        TArray<test_capital_ship_fighters::Simulation::Task> tasks;
        TArray<FVector3f> velocities;
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample{.capital_count = capitals.get_num_instances()};
        sample.tasks.Append(fighters.get_tasks());
        for (auto const handle : fighters.get_handles()) {
            sample.velocities.Add(harness.get_registry().get_velocity(handle));
        }
        samples.add(harness.get_time(), MoveTemp(sample));
    };
    harness.timeline.then_after(8.0, [&] { harness.queue_kills(TArray{enemy}); })
        .then_after(0.1, [] {})
        .finish_after(0.0);
    test.TestTrue(TEXT("Standby-transition timeline completes"),
                  harness.run_until_timeline_finished(9.0));
    checks.is_true(!samples.is_empty(), TEXT("Standby-transition samples are recorded"));
    if (samples.is_empty()) {
        return;
    }

    auto const& before{samples.nearest_value(8.0)};
    auto const& after{samples.nearest_value(8.1)};
    checks.is_greater_than(before.velocities.Num(), int32{0}, TEXT("Fighters spawned before kill"));
    checks.is_true(before.velocities.ContainsByPredicate(
                       [](FVector3f const velocity) { return !velocity.IsNearlyZero(); }),
                   TEXT("At least one fighter moves before standby"));
    checks.are_equal(1, after.capital_count, TEXT("One capital remains after kill"));
    checks.are_equal(after.tasks.Num(),
                     after.velocities.Num(),
                     TEXT("Standby tasks and velocities have matching counts"));
    for (int32 i{}; i < after.tasks.Num(); ++i) {
        checks.are_equal(test_capital_ship_fighters::Simulation::Task::Standby,
                         after.tasks[i],
                         TEXT("Fighter transitioned to standby"),
                         i);
        checks.dist_zero(after.velocities[i],
                         FVector3f::ZeroVector,
                         0.f,
                         TEXT("Standby fighter velocity is zero"),
                         i);
    }
}

}
