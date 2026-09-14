#include "test_simulation_core_regressions.h"
#include "../support/simulation_test_support.h"

#include <ioj/sim/capital_ships/sim.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/player/sim.h>

namespace ioj::sim {
namespace {
constexpr double nonlethal_damage_time{0.05};
constexpr double lethal_damage_time{0.15};
constexpr double damage_test_end_time{0.25};
constexpr std::int32_t initial_health{100};
}

namespace collision_damage_test {
inline constexpr std::int32_t player_health{1000};
inline constexpr std::int32_t capital_health{150};
inline constexpr std::int32_t overlap_damage{50};
inline constexpr double duration{0.1};
}

void run_worldless_simulation_core_regression(ioj::sim::tests::SimulationFixture const& config,
                                              SimulationCoreRegressionScenario const scenario) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    if (scenario == SimulationCoreRegressionScenario::DamageLifecycle) {
        data.capital_ships.fighter_spawn_slots = 0;
        data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
        ioj::sim::tests::add_capital_spawn(
            data, {}, ioj::sim::Team::White, -1, 60.f, 60.f, initial_health);
    }

    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};
    if (scenario == SimulationCoreRegressionScenario::FixedTickLifecycle) {
        std::int32_t end_tick_calls{};
        harness.on_end_tick = [&](LevelSim&) { ++end_tick_calls; };
        simulation.start();
        auto const period{simulation.get_clock().get_tick_period()};
        simulation.advance(period * 0.5);
        ioj::sim::tests::expect_equal(std::uint64_t{0},
                                      simulation.get_clock().get_completed_ticks(),
                                      "Half tick is accumulated");
        simulation.advance(period * 0.5);
        ioj::sim::tests::expect_equal(std::uint64_t{1},
                                      simulation.get_clock().get_completed_ticks(),
                                      "Two half ticks advance once");
        ioj::sim::tests::expect_equal(
            1, end_tick_calls, "End-tick hook executes once per completed tick");
        simulation.advance(period * 3.25);
        ioj::sim::tests::expect_equal(std::uint64_t{4},
                                      simulation.get_clock().get_completed_ticks(),
                                      "Large delta catches up deterministically");
        ioj::sim::tests::expect_equal(4, end_tick_calls, "Catch-up executes every end-tick hook");
        ioj::sim::tests::expect_equal(period * 4.0,
                                      simulation.get_clock().get_simulation_time(),
                                      1.e-9,
                                      "Simulation time derives from completed ticks");
        simulation.pause();
        simulation.advance(period * 10.0);
        ioj::sim::tests::expect_equal(std::uint64_t{4},
                                      simulation.get_clock().get_completed_ticks(),
                                      "Paused simulation ignores time");
        simulation.start();
        simulation.advance(period * 0.75);
        ioj::sim::tests::expect_equal(std::uint64_t{5},
                                      simulation.get_clock().get_completed_ticks(),
                                      "Resume preserves accumulated fraction");
        ioj::sim::tests::expect_equal(5, end_tick_calls, "Resumed tick executes one hook");
        return;
    }

    auto const damaged_handle{simulation.get_capital_ships().get_handle(0)};
    struct DamageSample {
        std::int32_t capital_count{};
        std::int32_t registry_alive_count{};
        std::int32_t health{};
        std::int32_t telemetry_active_count{};
    };
    ml::TimeSeriesData<DamageSample> samples;
    harness.on_end_tick = [&](LevelSim& level) {
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
                        [&] { harness.queue_damage(std::array{damaged_handle}, 25); });
    harness.timeline.at(lethal_damage_time,
                        [&] { harness.queue_damage(std::array{damaged_handle}, 75); });
    harness.timeline.finish_at(damage_test_end_time);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(1.0),
                                 "Damage lifecycle timeline completes");
    ioj::sim::tests::expect_true(!samples.is_empty(), "Damage lifecycle samples are recorded");
    if (samples.is_empty()) {
        return;
    }
    auto const& initial{samples.value_at(0)};
    auto const& nonlethal{samples.nearest_value(0.10)};
    auto const& lethal{samples.nearest_value(0.22)};
    ioj::sim::tests::expect_equal(1, initial.capital_count, "One capital starts active");
    ioj::sim::tests::expect_equal(
        initial_health, initial.health, "Capital starts at configured health");
    ioj::sim::tests::expect_equal(
        1, nonlethal.capital_count, "Nonlethal damage preserves batch entity");
    ioj::sim::tests::expect_equal(75, nonlethal.health, "Nonlethal damage is applied once");
    ioj::sim::tests::expect_equal(
        0, lethal.capital_count, "Lethal damage removes batch entity in resolution tick");
    ioj::sim::tests::expect_equal(
        0, lethal.registry_alive_count, "Registry death commits in the same tick");
    ioj::sim::tests::expect_equal(
        0, lethal.telemetry_active_count, "Telemetry observes committed death before hook");
    ioj::sim::tests::expect_equal(
        0, lethal.health, "Registry retains terminal health for the dead handle");
    ioj::sim::tests::expect_true(harness.get_registry().is_valid_dead(damaged_handle),
                                 "Killed handle remains valid-dead");
    ioj::sim::tests::expect_equal(
        0, harness.get_registry().count_kills(), "Unattributed death does not create a kill");
}

void run_worldless_collision_damage(ioj::sim::tests::SimulationFixture const& config) {
    struct Sample {
        std::int32_t player_health{};
        std::int32_t capital_health{};
        std::int32_t dynamic_overlap_count{};
        std::int32_t kill_count{};
        bool player_alive{};
    };

    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.overlap_response.damage_per_overlap_detection = collision_damage_test::overlap_damage;
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    ioj::sim::tests::add_player_spawn(data, ioj::sim::tests::make_player_spawn(config));
    data.player->config.lateral_adjustment_speed = 1.f;
    data.player->health = {collision_damage_test::player_health,
                           collision_damage_test::player_health};
    auto const player_bounds_index{ioj::sim::collision::EntityAABBs::space_ship_index};
    data.entity_bounds.half_extent_xs[player_bounds_index] = 1.f;
    data.entity_bounds.half_extent_ys[player_bounds_index] = 1.f;
    data.entity_bounds.half_extent_zs[player_bounds_index] = 1.f;
    ioj::sim::tests::add_capital_spawn(data,
                                       ioj::sim::Vector3f{},
                                       ioj::sim::Team::White,
                                       -1,
                                       60.f,
                                       60.f,
                                       collision_damage_test::capital_health);

    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};
    auto* const player{simulation.get_player_ship_simulation()};
    if (!ioj::sim::tests::expect_not_null(player,
                                          "Collision test player simulation is available")) {
        return;
    }

    auto const player_handle{player->registry_handle};
    auto const capital_handle{simulation.get_capital_ships().get_handle(0)};
    auto const capital_id{harness.get_registry().find_unique_id(capital_handle)};
    player->set_lateral_move_input(1.f);
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim& level) {
        auto const& registry{harness.get_registry()};
        auto const events{
            level.get_spatial_query_manager().get_collision_system().get_aabb_overlap_events()};
        samples.add(harness.get_time(),
                    Sample{
                        .player_health = registry.get_health(player_handle),
                        .capital_health = registry.get_health(capital_handle),
                        .dynamic_overlap_count = events.entity_entity_overlaps.num(),
                        .kill_count = registry.count_kills(),
                        .player_alive = registry.is_valid_alive(player_handle),
                    });
    };
    harness.timeline.finish_after(collision_damage_test::duration);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(2.0),
                                 "Collision damage timeline completes");
    ioj::sim::tests::expect_greater(samples.num(), 2, "Three collision ticks are recorded");
    if (::testing::Test::HasFailure()) {
        return;
    }

    auto const& first{samples.value_at(0)};
    auto const& second{samples.value_at(1)};
    auto const& third{samples.value_at(2)};
    ioj::sim::tests::expect_equal(1, first.dynamic_overlap_count, "First tick detects one overlap");
    ioj::sim::tests::expect_equal(
        1, second.dynamic_overlap_count, "Second tick detects one overlap");
    ioj::sim::tests::expect_equal(1, third.dynamic_overlap_count, "Third tick detects one overlap");
    ioj::sim::tests::expect_equal(collision_damage_test::player_health -
                                      collision_damage_test::overlap_damage,
                                  first.player_health,
                                  "First overlap damages the player");
    ioj::sim::tests::expect_equal(collision_damage_test::player_health -
                                      2 * collision_damage_test::overlap_damage,
                                  second.player_health,
                                  "Second overlap damages the player");
    ioj::sim::tests::expect_equal(collision_damage_test::player_health -
                                      3 * collision_damage_test::overlap_damage,
                                  third.player_health,
                                  "Third overlap damages the player");
    ioj::sim::tests::expect_equal(collision_damage_test::capital_health -
                                      collision_damage_test::overlap_damage,
                                  first.capital_health,
                                  "First overlap damages the capital");
    ioj::sim::tests::expect_equal(collision_damage_test::capital_health -
                                      2 * collision_damage_test::overlap_damage,
                                  second.capital_health,
                                  "Second overlap damages the capital");
    ioj::sim::tests::expect_equal(0, third.capital_health, "Third overlap kills the capital");
    ioj::sim::tests::expect_true(third.player_alive, "Player survives the third overlap tick");
    ioj::sim::tests::expect_true(harness.get_registry().is_valid_dead(capital_handle),
                                 "Capital death commits in the third overlap tick");
    ioj::sim::tests::expect_equal(0, third.kill_count, "Collision death grants no combat kill");
    ioj::sim::tests::expect_true(
        harness.get_registry().get_unique_entities().life_state[capital_id.id] ==
            ioj::sim::LifeState::Unknown,
        "Collision death is environmental");
}
}
