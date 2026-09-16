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

void run_worldless_simulation_core_regression(tests::SimulationFixture const& config,
                                              SimulationCoreRegressionScenario const scenario) {
    auto data{tests::make_simulation_data(config)};
    if (scenario == SimulationCoreRegressionScenario::DamageLifecycle) {
        data.capital_ships.fighter_spawn_slots = 0;
        data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
        tests::add_capital_spawn(data, {}, Team::White, -1, 60.f, 60.f, initial_health);
    }

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};
    if (scenario == SimulationCoreRegressionScenario::FixedTickLifecycle) {
        std::int32_t end_tick_calls{};
        harness.on_end_tick = [&](LevelSim&) { ++end_tick_calls; };
        simulation.start();
        auto const period{simulation.get_clock().get_tick_period()};
        harness.advance(period * 0.5);
        tests::expect_equal(std::uint64_t{0},
                            simulation.get_clock().get_completed_ticks(),
                            "Half tick is accumulated");
        harness.advance(period * 0.5);
        tests::expect_equal(std::uint64_t{1},
                            simulation.get_clock().get_completed_ticks(),
                            "Two half ticks advance once");
        tests::expect_equal(1, end_tick_calls, "End-tick hook executes once per completed tick");
        harness.advance(period * 3.25);
        tests::expect_equal(std::uint64_t{4},
                            simulation.get_clock().get_completed_ticks(),
                            "Large delta catches up deterministically");
        tests::expect_equal(
            2, end_tick_calls, "Catch-up is observed once after the completed advance");
        tests::expect_equal(period * 4.0,
                            simulation.get_clock().get_simulation_time(),
                            1.e-9,
                            "Simulation time derives from completed ticks");
        simulation.pause();
        harness.advance(period * 10.0);
        tests::expect_equal(std::uint64_t{4},
                            simulation.get_clock().get_completed_ticks(),
                            "Paused simulation ignores time");
        simulation.start();
        harness.advance(period * 0.75);
        tests::expect_equal(std::uint64_t{5},
                            simulation.get_clock().get_completed_ticks(),
                            "Resume preserves accumulated fraction");
        tests::expect_equal(3, end_tick_calls, "Resumed tick executes one hook");
        return;
    }

    auto const damaged_handle{simulation.get_capital_ships().get_id(0)};
    struct DamageSample {
        std::int32_t capital_count{};
        std::int32_t registry_alive_count{};
        std::int32_t health{};
        std::int32_t telemetry_active_count{};
    };
    ml::TimeSeriesData<DamageSample> samples;
    harness.on_end_tick = [&](LevelSim& level) {
        auto const& telemetry{level.get_level_telemetry_manager().get_active_entity_count_data()};
        auto const state{level.get_agent_accessor().read(damaged_handle)};
        samples.add(harness.get_time(),
                    DamageSample{
                        .capital_count = level.get_capital_ships().get_num_instances(),
                        .registry_alive_count = harness.get_registry().count_alive(),
                        .health = state ? state->health : 0,
                        .telemetry_active_count = telemetry.last_value(),
                    });
    };
    harness.timeline.at(nonlethal_damage_time,
                        [&] { harness.queue_damage(std::array{damaged_handle}, 25); });
    harness.timeline.at(lethal_damage_time,
                        [&] { harness.queue_damage(std::array{damaged_handle}, 75); });
    harness.timeline.finish_at(damage_test_end_time);
    tests::expect_true(harness.run_until_timeline_finished(1.0),
                       "Damage lifecycle timeline completes");
    tests::expect_true(!samples.is_empty(), "Damage lifecycle samples are recorded");
    if (samples.is_empty()) {
        return;
    }
    auto const& initial{samples.value_at(0)};
    auto const& nonlethal{samples.nearest_value(0.10)};
    auto const& lethal{samples.nearest_value(0.22)};
    tests::expect_equal(1, initial.capital_count, "One capital starts active");
    tests::expect_equal(initial_health, initial.health, "Capital starts at configured health");
    tests::expect_equal(1, nonlethal.capital_count, "Nonlethal damage preserves batch entity");
    tests::expect_equal(75, nonlethal.health, "Nonlethal damage is applied once");
    tests::expect_equal(
        0, lethal.capital_count, "Lethal damage removes batch entity in the same Action phase");
    tests::expect_equal(0, lethal.registry_alive_count, "Registry death commits in the same tick");
    tests::expect_equal(
        0, lethal.telemetry_active_count, "Telemetry observes committed death before hook");
    tests::expect_equal(0, lethal.health, "Registry retains terminal health for the dead handle");
    tests::expect_true(!harness.get_simulation().get_agent_accessor().is_alive(damaged_handle),
                       "Killed ID is dead");
    tests::expect_equal(
        0, harness.get_registry().count_kills(), "Unattributed death does not create a kill");
}

void run_worldless_collision_damage(tests::SimulationFixture const& config) {
    struct Sample {
        std::int32_t player_health{};
        std::int32_t capital_health{};
        std::int32_t dynamic_overlap_count{};
        std::int32_t kill_count{};
        bool player_alive{};
    };

    auto data{tests::make_simulation_data(config)};
    data.overlap_response.damage_per_overlap_detection = collision_damage_test::overlap_damage;
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    tests::add_player_spawn(data, tests::make_player_spawn(config));
    data.player->config.lateral_adjustment_speed = 1.f;
    data.player->health = {collision_damage_test::player_health,
                           collision_damage_test::player_health};
    auto const player_bounds_index{collision::EntityAABBs::space_ship_index};
    data.entity_bounds.set_half_extents(player_bounds_index, {{1.f, 1.f, 1.f}});
    tests::add_capital_spawn(
        data, Vector3f{}, Team::White, -1, 60.f, 60.f, collision_damage_test::capital_health);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};
    auto* const player{simulation.get_player_ship_simulation()};
    if (!tests::expect_not_null(player, "Collision test player simulation is available")) {
        return;
    }

    auto const player_id{player->unique_entity_id};
    auto const capital_id{simulation.get_capital_ships().get_id(0)};
    simulation.get_player_ship_commands()->set_lateral_move_input(1.f);
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim& level) {
        auto const events{
            level.get_spatial_query_manager().get_collision_system().get_aabb_overlap_events()};
        auto const capital{level.get_agent_accessor().read(capital_id)};
        samples.add(harness.get_time(),
                    Sample{
                        .player_health = player->health.health,
                        .capital_health = capital ? capital->health : 0,
                        .dynamic_overlap_count = events.entity_entity_overlaps.num(),
                        .kill_count = harness.get_registry().count_kills(),
                        .player_alive = level.get_agent_accessor().is_alive(player_id),
                    });
    };
    harness.timeline.finish_after(collision_damage_test::duration);
    tests::expect_true(harness.run_until_timeline_finished(2.0),
                       "Collision damage timeline completes");
    tests::expect_greater(samples.num(), 2, "Three collision ticks are recorded");
    if (::testing::Test::HasFailure()) {
        return;
    }

    auto const& first{samples.value_at(0)};
    auto const& second{samples.value_at(1)};
    auto const& third{samples.value_at(2)};
    tests::expect_equal(1, first.dynamic_overlap_count, "First tick detects one overlap");
    tests::expect_equal(1, second.dynamic_overlap_count, "Second tick detects one overlap");
    tests::expect_equal(1, third.dynamic_overlap_count, "Third tick detects one overlap");
    tests::expect_equal(collision_damage_test::player_health -
                            collision_damage_test::overlap_damage,
                        first.player_health,
                        "First overlap damages the player");
    tests::expect_equal(collision_damage_test::player_health -
                            2 * collision_damage_test::overlap_damage,
                        second.player_health,
                        "Second overlap damages the player");
    tests::expect_equal(collision_damage_test::player_health -
                            3 * collision_damage_test::overlap_damage,
                        third.player_health,
                        "Third overlap damages the player");
    tests::expect_equal(collision_damage_test::capital_health -
                            collision_damage_test::overlap_damage,
                        first.capital_health,
                        "First overlap damages the capital");
    tests::expect_equal(collision_damage_test::capital_health -
                            2 * collision_damage_test::overlap_damage,
                        second.capital_health,
                        "Second overlap damages the capital");
    tests::expect_equal(0, third.capital_health, "Third overlap kills the capital");
    tests::expect_true(third.player_alive, "Player survives the third overlap tick");
    tests::expect_true(!harness.get_simulation().get_agent_accessor().is_alive(capital_id),
                       "Capital death commits in the third overlap tick");
    tests::expect_equal(0, third.kill_count, "Collision death grants no combat kill");
    tests::expect_true(harness.get_registry()
                               .get_unique_entities()
                               .life_state[harness.get_registry().get_history_index(capital_id)] ==
                           LifeState::Unknown,
                       "Collision death is environmental");
}
}
