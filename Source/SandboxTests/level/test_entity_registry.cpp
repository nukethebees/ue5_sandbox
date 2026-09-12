#include "test_entity_registry.h"

#include <SandboxTests/support/SoftTestAssertions.h>

#include <SandboxCoreEngine/enums.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsConfig.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/entities/TestEntityRegistryData.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <SandboxCore/array_math.h>

namespace ml {
namespace {
constexpr TStaticArray<int32, 6> expected_team_counts{0, 1, 2, 3, 4, 5};
}

void run_worldless_entity_registry_scenario(FAutomationTestBase& test,
                                            FSoftTestAssertions& checks,
                                            USpaceGameLevelConfig const& config,
                                            EEntityRegistryScenario const scenario) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    if (scenario != EEntityRegistryScenario::TeamCounts) {
        data.player.Emplace(make_worldless_player_spawn(config));
    }
    int32 actor_index{};
    for (int32 team_index{}; team_index < expected_team_counts.Num(); ++team_index) {
        for (int32 i{}; i < expected_team_counts[team_index]; ++i) {
            auto const index{data.capital_spawns.num()};
            data.capital_spawns.add_defaulted(1);
            ml::assign(data.capital_spawns.locations,
                       index,
                       FVector{static_cast<float>(actor_index * 5000),
                               static_cast<float>(team_index * 5000),
                               4360.f});
            data.capital_spawns.teams[index] = static_cast<ETestTeam>(team_index);
            data.capital_spawns.healths[index] = data.capital_ships.max_health;
            data.capital_spawns.initial_spawn_delays[index] = 5.f;
            data.capital_spawns.spawn_cooldowns[index] = 60.f;
            ++actor_index;
        }
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    if (scenario == EEntityRegistryScenario::TeamCounts) {
        FTestEntityRegistry::TeamCounts counts{};
        FTestEntityRegistry::EntityCounts type_counts{};
        harness.on_end_tick = [&](FLevelSimulation&) {
            counts = harness.get_registry().count_alive_per_team();
            type_counts = harness.get_registry().count_alive_per_team_and_type();
        };
        harness.timeline.finish_at(0.1);
        test.TestTrue(TEXT("Team-count timeline completes"),
                      harness.run_until_timeline_finished(1.0));
        checks.are_equal(15, sum(TConstArrayView<int32>{counts}), TEXT("Check entity total"));
        for (int32 team_index{}; team_index < expected_team_counts.Num(); ++team_index) {
            auto const team{static_cast<ETestTeam>(team_index)};
            checks.are_equal(
                expected_team_counts[team_index],
                counts[team_index],
                FString::Printf(TEXT("Count team %s"), *to_string_without_type_prefix(team)));
            int32 type_count{};
            constexpr auto type_count_limit{EnumCountTrait<ETestEntityType>::count_value};
            for (int32 type{}; type < type_count_limit; ++type) {
                type_count += type_counts[team_index][type];
            }
            checks.are_equal(counts[team_index],
                             type_count,
                             FString::Printf(TEXT("Count team/type matrix for %s"),
                                             *to_string_without_type_prefix(team)));
        }
        return;
    }

    auto const expected_kills{scenario == EEntityRegistryScenario::OnePlayerKill ? 1 : 2};
    auto const* player{harness.get_simulation().get_player_ship_simulation()};
    check(player);
    auto const player_id{player->unique_entity_id};
    auto const player_handle{player->registry_handle};
    auto const initial_alive_count{harness.get_registry().count_alive()};
    auto const available_targets{harness.get_registry().get_handles_not_in_team(player->team)};
    checks.is_greater_than(available_targets.Num(),
                           expected_kills - 1,
                           TEXT("Enough non-player-team targets are available"));
    TArray<FRegistryEntityHandle> targets;
    targets.Append(available_targets.GetData(), expected_kills);
    struct Sample {
        int32 player_kills{};
        int32 total_kills{};
        int32 alive_count{};
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
        auto const& registry{harness.get_registry()};
        samples.add(harness.get_time(),
                    Sample{static_cast<int32>(registry.get_kills(player_id)),
                           registry.count_kills(),
                           registry.count_alive()});
    };
    harness.timeline.then_after(0.1, [&] { harness.queue_kills(targets, player_handle); });
    harness.timeline.finish_at(0.35);
    test.TestTrue(TEXT("Player-kill timeline completes"), harness.run_until_timeline_finished(1.0));
    checks.is_true(!samples.is_empty(), TEXT("Kill samples recorded"));
    if (samples.is_empty()) {
        return;
    }
    auto const& before{samples.nearest_value(0.05)};
    auto const& after{samples.nearest_value(0.3)};
    auto const& final{samples.nearest_value(0.35)};
    checks.are_equal(0, before.player_kills, TEXT("Player kills are zero before event"));
    checks.are_equal(0, before.total_kills, TEXT("Total kills are zero before event"));
    checks.are_equal(
        initial_alive_count, before.alive_count, TEXT("All entities are alive before event"));
    checks.are_equal(
        expected_kills, after.player_kills, TEXT("Kills are attributed to player ship"));
    checks.are_equal(
        expected_kills, after.total_kills, TEXT("Total kill count matches killed entities"));
    checks.are_equal(initial_alive_count - expected_kills,
                     after.alive_count,
                     TEXT("Alive count reflects killed entities"));
    checks.are_equal(expected_kills, final.player_kills, TEXT("Player kill count remains correct"));
    checks.are_equal(expected_kills, final.total_kills, TEXT("Total kill count remains correct"));
    checks.are_equal(initial_alive_count - expected_kills,
                     final.alive_count,
                     TEXT("Alive count remains correct"));
}

}
