#include "test_fighters_intercept_capital.h"

#include <SandboxTests/support/SoftTestAssertions.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>

#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestFightersInterceptCapitalResults.h>
#include <SandboxTests/support/TestResultAssetIO.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

namespace ml {
namespace fighters_intercept_test {
inline constexpr int32 collision_resilient_health{1'000'000};
}

void run_worldless_fighters_intercept_capital(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.health = fighters_intercept_test::collision_resilient_health;
    data.capital_spawns.add_defaulted(3);
    data.capital_spawns.locations.xs = {-61180.f, 77320.f, 3590.f};
    data.capital_spawns.locations.ys = {2170.f, 2170.f, 3240.f};
    data.capital_spawns.locations.zs = {4360.f, 4360.f, 4360.f};
    data.capital_spawns.teams = {ETestTeam::Green, ETestTeam::Red, ETestTeam::Blue};
    data.capital_spawns.healths.Init(fighters_intercept_test::collision_resilient_health, 3);
    data.capital_spawns.initial_spawn_delays = {0.f, 600.f, 600.f};
    data.capital_spawns.spawn_cooldowns = {60.f, 60.f, 60.f};
    data.capital_target_spawn_indices = {1, 0, 0};

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    auto const hero{capitals.get_handle(0)};
    auto const original_target{capitals.get_handle(1)};
    auto const intercept_target{capitals.get_handle(2)};

    struct Sample {
        FRegistryEntityHandle parent_target;
        TArray<FRegistryEntityHandle> fighter_targets;
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample;
        sample.parent_target = capitals.get_target_handle(0);
        for (auto const fighter_handle : capitals.get_fighter_handles(0)) {
            if (fighters.has_handle(fighter_handle)) {
                sample.fighter_targets.Add(fighters.get_target_handle(fighter_handle));
            }
        }
        samples.add(harness.get_time(), MoveTemp(sample));
    };
    harness.timeline.finish_at(20.0);
    test.TestTrue(TEXT("Fighter interception timeline completes"),
                  harness.run_until_timeline_finished(21.0));
    checks.is_true(!samples.is_empty(), TEXT("Fighter interception samples are recorded"));
    if (samples.is_empty()) {
        return;
    }

    auto const& start{samples.nearest_value(2.0 / 60.0)};
    auto const& end{samples.nearest_value(20.0)};
    checks.is_greater_than(start.fighter_targets.Num(), int32{0}, TEXT("Parent has fighters"));
    checks.is_greater_than(end.fighter_targets.Num(), int32{0}, TEXT("Parent has fighters at end"));
    checks.are_equal(
        original_target, start.parent_target, TEXT("Green capital initially targets red capital"));
    for (int32 i{}; i < start.fighter_targets.Num(); ++i) {
        checks.are_equal(original_target,
                         start.fighter_targets[i],
                         TEXT("Initial fighter target matches red parent target"),
                         i);
    }
    for (int32 i{}; i < end.fighter_targets.Num(); ++i) {
        checks.are_equal(intercept_target,
                         end.fighter_targets[i],
                         TEXT("Final fighter target is blue capital"),
                         i);
    }
    checks.are_equal(hero, capitals.get_handle(0), TEXT("Hero capital handle remains stable"));
}

}
