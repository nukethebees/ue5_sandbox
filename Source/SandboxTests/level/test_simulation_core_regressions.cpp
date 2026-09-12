#include "test_simulation_core_regressions.h"

#include <SandboxTests/support/SoftTestAssertions.h>

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipSimulation.h>

namespace ml {
namespace {
constexpr double nonlethal_damage_time{0.05};
constexpr double lethal_damage_time{0.15};
constexpr double damage_test_end_time{0.25};
constexpr int32 initial_health{100};
}

namespace collision_damage_test {
inline constexpr int32 player_health{1000};
inline constexpr int32 capital_health{150};
inline constexpr int32 overlap_damage{50};
inline constexpr double duration{0.1};
}

void run_worldless_simulation_core_regression(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config,
                                              ESimulationCoreRegressionScenario const scenario) {
    auto data{make_worldless_simulation_test_data(config)};
    if (scenario == ESimulationCoreRegressionScenario::DamageLifecycle) {
        data.capital_ships.fighter_spawn_slots = 0;
        data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
        data.capital_spawns.add_defaulted(1);
        data.capital_spawns.teams[0] = ETestTeam::White;
        data.capital_spawns.healths[0] = initial_health;
        data.capital_spawns.initial_spawn_delays[0] = 60.f;
        data.capital_spawns.spawn_cooldowns[0] = 60.f;
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};
    if (scenario == ESimulationCoreRegressionScenario::FixedTickLifecycle) {
        int32 end_tick_calls{};
        harness.on_end_tick = [&](FLevelSimulation&) { ++end_tick_calls; };
        simulation.start();
        auto const period{simulation.get_clock().get_tick_period()};
        simulation.advance(period * 0.5);
        checks.are_equal(uint64{0},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Half tick is accumulated"));
        simulation.advance(period * 0.5);
        checks.are_equal(uint64{1},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Two half ticks advance once"));
        checks.are_equal(1, end_tick_calls, TEXT("End-tick hook executes once per completed tick"));
        simulation.advance(period * 3.25);
        checks.are_equal(uint64{4},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Large delta catches up deterministically"));
        checks.are_equal(4, end_tick_calls, TEXT("Catch-up executes every end-tick hook"));
        checks.are_equal(period * 4.0,
                         simulation.get_clock().get_simulation_time(),
                         1.e-9,
                         TEXT("Simulation time derives from completed ticks"));
        simulation.pause();
        simulation.advance(period * 10.0);
        checks.are_equal(uint64{4},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Paused simulation ignores time"));
        simulation.start();
        simulation.advance(period * 0.75);
        checks.are_equal(uint64{5},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Resume preserves accumulated fraction"));
        checks.are_equal(5, end_tick_calls, TEXT("Resumed tick executes one hook"));
        return;
    }

    auto const damaged_handle{simulation.get_capital_ships().get_handle(0)};
    struct DamageSample {
        int32 capital_count{};
        int32 registry_alive_count{};
        int32 health{};
        int32 telemetry_active_count{};
    };
    TimeSeriesData<DamageSample> samples;
    harness.on_end_tick = [&](FLevelSimulation& level) {
        auto const& registry{harness.get_registry()};
        auto const& telemetry{level.get_level_telemetry_manager().get_active_entity_count_data()};
        samples.add(harness.get_time(),
                    DamageSample{
                        .capital_count = level.get_capital_ships().get_num_instances(),
                        .registry_alive_count = registry.count_alive(),
                        .health = registry.is_valid_handle(damaged_handle)
                                    ? registry.get_health(damaged_handle)
                                    : 0,
                        .telemetry_active_count = telemetry.last_value(),
                    });
    };
    harness.timeline.at(nonlethal_damage_time,
                        [&] { harness.queue_damage(TArray{damaged_handle}, 25); });
    harness.timeline.at(lethal_damage_time,
                        [&] { harness.queue_damage(TArray{damaged_handle}, 75); });
    harness.timeline.finish_at(damage_test_end_time);
    test.TestTrue(TEXT("Damage lifecycle timeline completes"),
                  harness.run_until_timeline_finished(1.0));
    checks.is_true(!samples.is_empty(), TEXT("Damage lifecycle samples are recorded"));
    if (samples.is_empty()) {
        return;
    }
    auto const& initial{samples.value_at(0)};
    auto const& nonlethal{samples.nearest_value(0.10)};
    auto const& lethal{samples.nearest_value(0.22)};
    checks.are_equal(1, initial.capital_count, TEXT("One capital starts active"));
    checks.are_equal(initial_health, initial.health, TEXT("Capital starts at configured health"));
    checks.are_equal(1, nonlethal.capital_count, TEXT("Nonlethal damage preserves batch entity"));
    checks.are_equal(75, nonlethal.health, TEXT("Nonlethal damage is applied once"));
    checks.are_equal(
        0, lethal.capital_count, TEXT("Lethal damage removes batch entity in resolution tick"));
    checks.are_equal(
        0, lethal.registry_alive_count, TEXT("Registry death commits in the same tick"));
    checks.are_equal(
        0, lethal.telemetry_active_count, TEXT("Telemetry observes committed death before hook"));
    checks.are_equal(
        0, lethal.health, TEXT("Registry retains terminal health for the dead handle"));
    checks.is_true(harness.get_registry().is_valid_dead(damaged_handle),
                   TEXT("Killed handle remains valid-dead"));
    checks.are_equal(
        0, harness.get_registry().count_kills(), TEXT("Unattributed death does not create a kill"));
}

void run_worldless_collision_damage(FAutomationTestBase& test,
                                    FSoftTestAssertions& checks,
                                    USpaceGameLevelConfig const& config) {
    struct FSample {
        int32 player_health{};
        int32 capital_health{};
        int32 dynamic_overlap_count{};
        int32 kill_count{};
        bool player_alive{};
    };

    auto data{make_worldless_simulation_test_data(config)};
    data.overlap_response.damage_per_overlap_detection = collision_damage_test::overlap_damage;
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    data.player = make_worldless_player_spawn(config);
    data.player->config.lateral_adjustment_speed = 1.f;
    data.player->health = {collision_damage_test::player_health,
                           collision_damage_test::player_health};
    auto const player_bounds_index{ml::ioj::FEntityAABBs::space_ship_index};
    data.entity_bounds.half_extent_xs[player_bounds_index] = 1.f;
    data.entity_bounds.half_extent_ys[player_bounds_index] = 1.f;
    data.entity_bounds.half_extent_zs[player_bounds_index] = 1.f;
    add_worldless_capital_spawn(data,
                                FVector3f::ZeroVector,
                                ETestTeam::White,
                                INDEX_NONE,
                                60.f,
                                60.f,
                                collision_damage_test::capital_health);

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};
    auto* const player{simulation.get_player_ship_simulation()};
    if (!checks.not_nullptr(player, TEXT("Collision test player simulation is available"))) {
        return;
    }

    auto const player_handle{player->registry_handle};
    auto const capital_handle{simulation.get_capital_ships().get_handle(0)};
    auto const capital_id{harness.get_registry().find_unique_id(capital_handle)};
    player->set_lateral_move_input(1.f);
    TimeSeriesData<FSample> samples;
    harness.on_end_tick = [&](FLevelSimulation& level) {
        auto const& registry{harness.get_registry()};
        auto const events{
            level.get_spatial_query_manager().get_collision_system().get_aabb_overlap_events()};
        samples.add(harness.get_time(),
                    FSample{
                        .player_health = registry.get_health(player_handle),
                        .capital_health = registry.get_health(capital_handle),
                        .dynamic_overlap_count = events.entity_entity_overlaps.num(),
                        .kill_count = registry.count_kills(),
                        .player_alive = registry.is_valid_alive(player_handle),
                    });
    };
    harness.timeline.finish_after(collision_damage_test::duration);
    test.TestTrue(TEXT("Collision damage timeline completes"),
                  harness.run_until_timeline_finished(2.0));
    checks.is_greater_than(samples.num(), 2, TEXT("Three collision ticks are recorded"));
    if (!checks.all_passed) {
        return;
    }

    auto const& first{samples.value_at(0)};
    auto const& second{samples.value_at(1)};
    auto const& third{samples.value_at(2)};
    checks.are_equal(1, first.dynamic_overlap_count, TEXT("First tick detects one overlap"));
    checks.are_equal(1, second.dynamic_overlap_count, TEXT("Second tick detects one overlap"));
    checks.are_equal(1, third.dynamic_overlap_count, TEXT("Third tick detects one overlap"));
    checks.are_equal(collision_damage_test::player_health - collision_damage_test::overlap_damage,
                     first.player_health,
                     TEXT("First overlap damages the player"));
    checks.are_equal(collision_damage_test::player_health -
                         2 * collision_damage_test::overlap_damage,
                     second.player_health,
                     TEXT("Second overlap damages the player"));
    checks.are_equal(collision_damage_test::player_health -
                         3 * collision_damage_test::overlap_damage,
                     third.player_health,
                     TEXT("Third overlap damages the player"));
    checks.are_equal(collision_damage_test::capital_health - collision_damage_test::overlap_damage,
                     first.capital_health,
                     TEXT("First overlap damages the capital"));
    checks.are_equal(collision_damage_test::capital_health -
                         2 * collision_damage_test::overlap_damage,
                     second.capital_health,
                     TEXT("Second overlap damages the capital"));
    checks.are_equal(0, third.capital_health, TEXT("Third overlap kills the capital"));
    checks.is_true(third.player_alive, TEXT("Player survives the third overlap tick"));
    checks.is_true(harness.get_registry().is_valid_dead(capital_handle),
                   TEXT("Capital death commits in the third overlap tick"));
    checks.are_equal(0, third.kill_count, TEXT("Collision death grants no combat kill"));
    checks.is_true(harness.get_registry().get_unique_entities().death_reason[capital_id.id] ==
                       ETestDeathReason::Unknown,
                   TEXT("Collision death is environmental"));
}
}
