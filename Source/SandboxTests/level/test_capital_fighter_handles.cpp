#include "test_capital_fighter_handles.h"

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipsConfig.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersConfig.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/fighters/TestCapitalShipFightersSimulation.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/SoftTestAssertions.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestResultAssetIO.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <SandboxCore/time_series_data.h>

#include <Containers/Set.h>
#include <Misc/Optional.h>

/*
This test relies on a long spawn delay to ensure more fighters are not spawned.
The assumption is that there is one wave of fighters total.
*/

namespace ml {
namespace capital_fighter_handles_test {
inline constexpr int32 collision_resilient_health{1'000'000};
}

/* **************************************** */
// Simultaneous capital deaths
/* **************************************** */
void run_worldless_simultaneous_capital_reassignment(FAutomationTestBase& test,
                                                     FSoftTestAssertions& checks,
                                                     USpaceGameLevelConfig const& config) {
    struct FSample {
        TArray<FRegistryEntityHandle> capitals;
        TArray<TArray<FRegistryEntityHandle>> owned_fighters;
        TArray<ETestTeam> capital_teams;
        TArray<ETestTeam> fighter_teams;
        TArray<int32> span_starts;
        TArray<FRegistryEntityHandle> all_owned_fighters;
        TArray<FRegistryEntityHandle> fighters;
    };

    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.spawn_delay = 6000.f;
    data.capital_ships.max_health = capital_fighter_handles_test::collision_resilient_health;
    data.fighters.laser.damage = 0;
    data.fighters.health = capital_fighter_handles_test::collision_resilient_health;
    for (int32 i{}; i < 4; ++i) {
        add_worldless_capital_spawn(
            data,
            FVector3f{static_cast<float>((i % 2 == 0 ? -1.0 : 1.0) * 250000.0),
                      static_cast<float>((i / 2) * 200000.0),
                      0.f},
            i % 2 == 0 ? ETestTeam::Green : ETestTeam::Red,
            i ^ 1,
            0.f,
            6000.f,
            capital_fighter_handles_test::collision_resilient_health);
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    TimeSeriesData<FSample> samples;
    TArray<FRegistryEntityHandle> killed_capitals;
    harness.on_end_tick = [&](FLevelSimulation& simulation) {
        auto const& capitals{simulation.get_capital_ships()};
        auto const& registry{simulation.get_entity_registry()};
        FSample sample;
        auto const count{capitals.get_num_instances()};
        for (int32 i{}; i < count; ++i) {
            auto const handle{capitals.get_handle(i)};
            sample.capitals.Add(handle);
            sample.capital_teams.Add(registry.get_team(handle));
            sample.span_starts.Add(capitals.get_capital_fighter_handle_span(i).start());
            sample.owned_fighters.Emplace(capitals.get_fighter_handles(i));
            for (auto const fighter : capitals.get_fighter_handles(i)) {
                sample.fighter_teams.Add(registry.get_team(fighter));
            }
        }
        sample.all_owned_fighters.Append(capitals.get_fighter_handles());
        sample.fighters.Append(simulation.get_capital_ship_fighters().get_handles());
        samples.add(harness.get_time(), MoveTemp(sample));
    };
    harness.timeline.at(1.0, [&] {
        auto const& capitals{harness.get_simulation().get_capital_ships()};
        auto const& registry{harness.get_registry()};
        for (auto const team : {ETestTeam::Green, ETestTeam::Red}) {
            FRegistryEntityHandle victim;
            auto const count{capitals.get_num_instances()};
            for (int32 i{}; i < count; ++i) {
                auto const handle{capitals.get_handle(i)};
                if (registry.get_team(handle) == team) {
                    victim = handle;
                    if (team == ETestTeam::Red) {
                        break;
                    }
                }
            }
            if (checks.is_true(victim.is_valid(), TEXT("Team has a capital to kill"))) {
                killed_capitals.Add(victim);
            }
        }
        harness.queue_kills(killed_capitals);
    });
    harness.timeline.finish_at(2.0);
    test.TestTrue(TEXT("Capital reassignment timeline completes"),
                  harness.run_until_timeline_finished(10.0));
    checks.is_true(samples.num() > 1, TEXT("Capital reassignment samples are recorded"));
    if (!checks.all_passed) {
        return;
    }

    auto const& before{samples.value_at(samples.nearest_index(0.5))};
    auto const& after{samples.values().Last()};
    checks.are_equal(4, before.capitals.Num(), TEXT("Two capitals per team spawn"));
    checks.are_equal(2, after.capitals.Num(), TEXT("Both killed capitals are removed"));
    checks.is_greater_than(before.fighters.Num(), 0, TEXT("One fighter wave spawns"));
    checks.are_equal(before.fighters.Num(), after.fighters.Num(), TEXT("All fighters survive"));
    TSet<FRegistryEntityHandle> seen;
    int32 offset{};
    auto const count{after.capitals.Num()};
    for (int32 i{}; i < count; ++i) {
        checks.is_true(!killed_capitals.Contains(after.capitals[i]),
                       TEXT("Owner capital survives"));
        checks.are_equal(offset, after.span_starts[i], TEXT("Ownership spans are contiguous"));
        int32 expected_count{};
        auto const before_count{before.capitals.Num()};
        for (int32 j{}; j < before_count; ++j) {
            if (before.capital_teams[j] == after.capital_teams[i]) {
                expected_count += before.owned_fighters[j].Num();
            }
        }
        checks.are_equal(expected_count,
                         after.owned_fighters[i].Num(),
                         TEXT("Survivor owns both original waves on its team"));
        for (auto const fighter : after.owned_fighters[i]) {
            checks.is_true(!seen.Contains(fighter), TEXT("Fighter ownership is unique"));
            seen.Add(fighter);
            checks.is_true(before.fighters.Contains(fighter),
                           TEXT("Original fighter is preserved"));
            checks.is_true(after.fighters.Contains(fighter),
                           TEXT("Owned fighter is in simulation"));
            checks.are_equal(after.capital_teams[i],
                             after.fighter_teams[offset],
                             TEXT("Fighter belongs to owner's team"));
            if (checks.is_true(after.all_owned_fighters.IsValidIndex(offset),
                               TEXT("Span is within flat ownership array"))) {
                checks.are_equal(fighter,
                                 after.all_owned_fighters[offset],
                                 TEXT("Span matches flat ownership array"));
            }
            ++offset;
        }
    }
    checks.are_equal(after.fighters.Num(), seen.Num(), TEXT("Every fighter has one owner"));
    checks.are_equal(offset, after.all_owned_fighters.Num(), TEXT("Spans cover ownership array"));
}

/* **************************************** */
// Capital fighter handle lifecycle
/* **************************************** */
void run_worldless_capital_fighter_handles(FAutomationTestBase& test,
                                           FSoftTestAssertions& checks,
                                           USpaceGameLevelConfig const& config,
                                           ECapitalFighterHandlesScenario const scenario) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.spawn_delay = 10.f;
    data.capital_ships.max_health = 10000;
    data.fighters.speed = 2000.f;
    data.fighters.laser.max_distance = 15000.f;
    auto const green_index{add_worldless_capital_spawn(
        data, {-260800.f, -5060.f, 4360.f}, ETestTeam::Green, 1, 0.f, 6000.f)};
    add_worldless_capital_spawn(
        data, {245260.f, -451630.f, 4360.f}, ETestTeam::Red, green_index, 0.f, 6000.f);
    add_worldless_capital_spawn(
        data, {300450.f, 214000.f, 4360.f}, ETestTeam::Red, green_index, 0.f, 6000.f);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const& fighters{harness.get_simulation().get_capital_ship_fighters()};
    TArray<FRegistryEntityHandle> destroyed;
    TArray<FRegistryEntityHandle> kept;
    TArray<FRegistryEntityHandle> green_fighters_before_capital_kill;
    auto initial_checked{false};
    auto fighter_kill_checked{scenario == ECapitalFighterHandlesScenario::KillCapital};
    auto capital_kill_checked{scenario == ECapitalFighterHandlesScenario::KillFightersOnly};

    harness.timeline.at(0.2, [&] {
        checks.are_equal(3, capitals.get_num_instances(), TEXT("Three capitals are registered"));
        auto const expected_fighters{3 * capitals.get_fighter_spawn_slots()};
        checks.are_equal(expected_fighters,
                         fighters.get_num_instances(),
                         TEXT("Every capital spawns its fighter slots"));
        checks.are_equal(expected_fighters,
                         capitals.get_fighter_handles().Num(),
                         TEXT("Capital-owned and simulation fighter counts match"));
        for (int32 i{}; i < capitals.get_num_instances(); ++i) {
            checks.not_equal(capitals.get_handle(i),
                             capitals.get_target_handle(i),
                             TEXT("Capital does not target itself"),
                             i);
        }
        for (auto const target : fighters.get_target_handles()) {
            checks.is_true(target.is_valid(), TEXT("Spawned fighter has a target"));
        }
        initial_checked = true;
    });

    auto next_time{0.4};
    if (scenario != ECapitalFighterHandlesScenario::KillCapital) {
        harness.timeline.at(next_time, [&] {
            auto const handles{capitals.get_fighter_handles()};
            for (int32 i{}; i < handles.Num(); ++i) {
                (i % 2 == 0 ? destroyed : kept).Add(handles[i]);
            }
            harness.queue_kills(destroyed);
        });
        next_time += 0.2;
        harness.timeline.at(next_time, [&] {
            checks.are_equal(kept.Num(),
                             fighters.get_num_instances(),
                             TEXT("Killed fighters are removed from the simulation"));
            checks.are_equal(kept.Num(),
                             capitals.get_fighter_handles().Num(),
                             TEXT("Killed fighters are removed from capital ownership"));
            for (auto const handle : destroyed) {
                checks.is_true(harness.get_registry().is_valid_dead(handle),
                               TEXT("Destroyed fighter is dead"));
            }
            fighter_kill_checked = true;
        });
    }

    if (scenario != ECapitalFighterHandlesScenario::KillFightersOnly) {
        next_time += 0.2;
        harness.timeline.at(next_time, [&] {
            auto const main_index{capitals.find_first_index_on_team(ETestTeam::Green)};
            check(main_index.has_value());
            green_fighters_before_capital_kill =
                TArray<FRegistryEntityHandle>{capitals.get_fighter_handles(*main_index)};
            harness.queue_kills(TArray{capitals.get_target_handle(*main_index)});
        });
        next_time += 0.5;
        harness.timeline.at(next_time, [&] {
            checks.are_equal(2,
                             capitals.get_num_instances(),
                             TEXT("Killed capital is removed from the simulation"));
            auto const main_index{capitals.find_first_index_on_team(ETestTeam::Green)};
            checks.is_true(main_index.has_value(), TEXT("Green capital survives"));
            if (main_index.has_value()) {
                auto const remaining{capitals.get_fighter_handles(*main_index)};
                checks.are_equal(green_fighters_before_capital_kill.Num(),
                                 remaining.Num(),
                                 TEXT("Surviving capital keeps its fighters"));
                for (auto const handle : remaining) {
                    checks.is_true(green_fighters_before_capital_kill.Contains(handle),
                                   TEXT("Surviving fighter retains capital ownership"));
                    checks.is_true(fighters.get_target_handle(handle).is_valid(),
                                   TEXT("Surviving fighter retargets"));
                }
            }
            capital_kill_checked = true;
        });
    }
    harness.timeline.finish_at(next_time + 0.1);
    test.TestTrue(TEXT("Capital fighter handle timeline completes"),
                  harness.run_until_timeline_finished(next_time + 0.2));
    checks.is_true(initial_checked, TEXT("Initial fighter ownership is checked"));
    checks.is_true(fighter_kill_checked, TEXT("Fighter removal is checked"));
    checks.is_true(capital_kill_checked, TEXT("Capital removal is checked"));
}
}
