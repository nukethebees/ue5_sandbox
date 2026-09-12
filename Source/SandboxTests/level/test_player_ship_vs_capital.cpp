#include "test_player_ship_vs_capital.h"

#include <SandboxTests/support/SoftTestAssertions.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestPlayerShipVsCapitalResults.h>
#include <SandboxTests/support/TestResultAssetIO.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <Engine/DataTable.h>

namespace ml {
namespace player_vs_capital_test {
inline constexpr int32 collision_resilient_health{1'000'000};
}

void run_worldless_player_ship_vs_capital(FAutomationTestBase& test,
                                          FSoftTestAssertions& checks,
                                          USpaceGameLevelConfig const& config) {
    auto data{make_worldless_simulation_test_data(config)};
    data.fighters.health = player_vs_capital_test::collision_resilient_health;
    data.fighters.laser.projectile_speed = 20000.f;
    data.fighters.laser.max_distance = 1.f;
    data.player.Emplace(make_worldless_player_spawn(
        config, FTransform{FRotator{0.f, -90.f, 0.f}, FVector{19850.f, 1300.f, 980.f}}));
    data.player->health.health = player_vs_capital_test::collision_resilient_health;
    add_worldless_capital_spawn(data,
                                FVector3f{-22020.f, 2170.f, 4360.f},
                                ETestTeam::Green,
                                FLevelSimulationInitData::player_target_spawn_index,
                                0.f,
                                120.f);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto* const player{harness.get_simulation().get_player_ship_simulation()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    check(player);
    player->set_flight_mode(ETestSpaceShipFlightMode::ForwardSpeed);
    player->start_boost();
    auto const player_handle{player->registry_handle};
    struct Sample {
        FVector player_location;
        FVector registry_location;
        TArray<FVector3f> fighter_target_locations;
        TArray<FVector3f> fighter_locations;
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        Sample sample{.player_location = player->transform.GetLocation(),
                      .registry_location =
                          FVector{harness.get_registry().get_location(player_handle)}};
        sample.fighter_target_locations = to_vector3f_array(fighters.get_target_locations());
        sample.fighter_locations = to_vector3f_array(fighters.get_locations());
        samples.add(harness.get_time(), MoveTemp(sample));
    };
    harness.timeline.finish_at(5.6);
    test.TestTrue(TEXT("Player-versus-capital timeline completes"),
                  harness.run_until_timeline_finished(6.0));
    checks.is_true(!samples.is_empty(), TEXT("Player-versus-capital samples are recorded"));
    if (samples.is_empty()) {
        return;
    }

    auto const& settled{samples.nearest_value(0.1)};
    auto const& tracked{samples.nearest_value(0.6)};
    auto const& before_end{samples.nearest_value(5.1)};
    auto const& end{samples.nearest_value(5.6)};
    checks.dist_zero(settled.player_location,
                     settled.registry_location,
                     1.0,
                     TEXT("Registry and player locations match initially"));
    checks.dist_zero(tracked.player_location,
                     tracked.registry_location,
                     1.0,
                     TEXT("Registry and player locations match after movement"));
    checks.not_dist_zero(
        settled.player_location, tracked.player_location, 1.0, TEXT("Player ship moves"));
    checks.is_greater_than(
        tracked.fighter_target_locations.Num(), int32{0}, TEXT("Fighters have target locations"));
    checks.are_equal(tracked.fighter_target_locations.Num(),
                     end.fighter_target_locations.Num(),
                     TEXT("Fighter target count remains stable"));
    checks.are_equal(end.fighter_target_locations.Num(),
                     end.fighter_locations.Num(),
                     TEXT("Fighter and target counts match"));
    if (!checks.all_passed) {
        return;
    }
    auto const count{end.fighter_locations.Num()};
    for (int32 i{}; i < count; ++i) {
        checks.not_dist_zero(tracked.fighter_target_locations[i],
                             end.fighter_target_locations[i],
                             1.f,
                             TEXT("Fighter target follows player"),
                             i);
        checks.dist_greater_than(before_end.fighter_locations[i],
                                 end.fighter_locations[i],
                                 500.f,
                                 TEXT("Fighter moves late in simulation"),
                                 i);
        checks.dist_greater_than(before_end.fighter_target_locations[i],
                                 end.fighter_target_locations[i],
                                 500.f,
                                 TEXT("Fighter target updates late in simulation"),
                                 i);
    }
}

}
