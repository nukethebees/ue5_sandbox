#include <sandbox/simulation/combat/lasers/TestLasersSimulation.h>
#include <sandbox/simulation/defences/turrets/TestStaticTurretsSimulation.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include "../support/simulation_test_support.h"

#include "test_turrets_kill_one.h"

namespace ml {
namespace {
void add_worldless_turrets(FLevelSimulationInitData& data,
                           std::span<ml::simulation::Vector3f const> const locations,
                           std::span<ml::simulation::Team const> const teams,
                           std::int32_t const laser_damage,
                           std::int32_t const blue_health = -1) {
    assert(static_cast<std::int32_t>(locations.size()) == static_cast<std::int32_t>(teams.size()));
    auto const count{static_cast<std::int32_t>(locations.size())};
    data.turret_spawns.add_defaulted(count);
    data.turret_transforms.resize(static_cast<std::size_t>(count));
    for (std::int32_t i{}; i < count; ++i) {
        auto const health{blue_health != -1 && teams[i] == ml::simulation::Team::Blue
                              ? blue_health
                              : data.turrets.max_health};
        data.turret_spawns.locations.set(i, locations[i]);
        data.turret_spawns.teams[i] = static_cast<ml::simulation::Team>(teams[i]);
        data.turret_spawns.healths[i] = health;
        data.turret_spawns.laser_damages[i] = laser_damage;
        data.turret_transforms[i].location = {locations[i].X, locations[i].Y, locations[i].Z};
    }
}
}

void run_worldless_turret_combat(ml::simulation_tests::SimulationFixture const& config,
                                 ETurretCombatScenario const scenario) {
    static std::vector<ml::simulation::Vector3f> const locations{{{3160.f, -3200.f, 40.f}},
                                                                 {{1710.f, -3200.f, 40.f}},
                                                                 {{190.f, -3200.f, 40.f}},
                                                                 {{-1050.f, -3200.f, 40.f}},
                                                                 {{-2380.f, -3200.f, 40.f}},
                                                                 {{-3550.f, -3200.f, 40.f}},
                                                                 {{190.f, 2280.f, 40.f}}};
    static std::vector<ml::simulation::Team> const teams{ml::simulation::Team::Blue,
                                                         ml::simulation::Team::Blue,
                                                         ml::simulation::Team::Blue,
                                                         ml::simulation::Team::Blue,
                                                         ml::simulation::Team::Blue,
                                                         ml::simulation::Team::Blue,
                                                         ml::simulation::Team::Red};
    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const damage{scenario == ETurretCombatScenario::ZeroDamage ? 0
                                                                    : data.turrets.laser.damage};
    add_worldless_turrets(
        data, locations, teams, damage, scenario == ETurretCombatScenario::KillEnemy ? 100000 : -1);

    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    std::vector<std::int32_t> initial_healths{};
    auto const& registry{harness.get_registry()};
    auto const initial_count{registry.get_num_elements()};
    initial_healths.reserve(initial_count);
    for (std::int32_t i{}; i < initial_count; ++i) {
        initial_healths.push_back(registry.get_entity_data().healths[i]);
    }
    harness.timeline.finish_at(3.0);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(3.5),
                                      "Turret combat timeline completes");

    if (scenario == ETurretCombatScenario::KillEnemy) {
        ml::simulation_tests::expect_equal(1, registry.count_kills(), "One turret is killed");
        ml::simulation_tests::expect_equal(6, registry.count_alive(), "Hero turrets remain alive");
        for (auto const target : harness.get_simulation().get_turrets().get_target_handles()) {
            ml::simulation_tests::expect_true(target.is_null(),
                                              "Targets clear after the enemy dies");
        }
        return;
    }

    ml::simulation_tests::expect_equal(
        initial_count, registry.count_alive(), "Zero-damage turrets remain alive");
    for (std::int32_t i{}; i < initial_count; ++i) {
        ml::simulation_tests::expect_equal(initial_healths[i],
                                           registry.get_entity_data().healths[i],
                                           "Zero-damage combat preserves health",
                                           i);
    }
}

void run_worldless_turret_line_of_sight_blocking(
    ml::simulation_tests::SimulationFixture const& config) {
    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    std::vector<ml::simulation::Vector3f> const locations{{{-5000.f, 0.f, 0.f}},
                                                          {{5000.f, 0.f, 0.f}}};
    std::vector<ml::simulation::Team> const teams{ml::simulation::Team::Blue,
                                                  ml::simulation::Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    std::int32_t spawn_count_before_blocker{-1};
    harness.timeline
        .at(2.0,
            [&] {
                spawn_count_before_blocker =
                    harness.get_simulation().get_lasers().get_number_spawned();
                harness.get_simulation()
                    .get_spatial_query_manager()
                    .get_collision_system()
                    .get_uniform_grid()
                    .add_static_aabb({{-500.f, -3000.f, -3000.f}}, {{500.f, 3000.f, 3000.f}});
            })
        .finish_at(4.0);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(4.5),
                                      "Turret line-of-sight timeline completes");
    ml::simulation_tests::expect_greater(
        spawn_count_before_blocker, 0, "Turrets fire before blocking");
    ml::simulation_tests::expect_equal(spawn_count_before_blocker,
                                       harness.get_simulation().get_lasers().get_number_spawned(),
                                       "Turrets stop firing after line of sight is blocked");
}

void run_worldless_turret_search_requires_line_of_sight(
    ml::simulation_tests::SimulationFixture const& config) {
    auto data{ml::simulation_tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    std::vector<ml::simulation::Vector3f> const locations{
        {{-1000.f, 0.f, 0.f}}, {{1000.f, 0.f, 0.f}}, {{1000.f, 1000.f, 0.f}}};
    std::vector<ml::simulation::Team> const teams{
        ml::simulation::Team::Blue, ml::simulation::Team::Red, ml::simulation::Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    ml::simulation_tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    harness.get_simulation()
        .get_spatial_query_manager()
        .get_collision_system()
        .get_uniform_grid()
        .add_static_aabb({{-100.f, -300.f, -3000.f}}, {{100.f, 300.f, 3000.f}});
    harness.timeline.finish_at(1.0);
    ml::simulation_tests::expect_true(harness.run_until_timeline_finished(1.5),
                                      "Turret search timeline completes");
    auto const targets{harness.get_simulation().get_turrets().get_target_handles()};
    ml::simulation_tests::expect_equal(
        3, static_cast<std::int32_t>(targets.size()), "All turret targets are available");
    if (static_cast<std::int32_t>(targets.size()) != 3) {
        return;
    }
    ml::simulation_tests::expect_true(targets[0].is_valid(),
                                      "Blue turret selects a visible target");
    if (targets[0].is_valid()) {
        ml::simulation_tests::expect_distance_near(ml::simulation::Vector3f{{1000.f, 1000.f, 0.f}},
                                                   harness.get_registry().get_location(targets[0]),
                                                   1.f,
                                                   "Blue turret skips the blocked enemy");
    }
}

}
