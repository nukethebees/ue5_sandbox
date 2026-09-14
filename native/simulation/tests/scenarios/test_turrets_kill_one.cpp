#include <ioj/sim/entity_registry.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/turrets/sim.h>
#include "../support/simulation_test_support.h"

#include "test_turrets_kill_one.h"

namespace ioj::sim {
namespace {
void add_worldless_turrets(LevelSimInitData& data,
                           std::span<ioj::sim::Vector3f const> const locations,
                           std::span<ioj::sim::Team const> const teams,
                           std::int32_t const laser_damage,
                           std::int32_t const blue_health = -1) {
    assert(static_cast<std::int32_t>(locations.size()) == static_cast<std::int32_t>(teams.size()));
    auto const count{static_cast<std::int32_t>(locations.size())};
    for (std::int32_t i{}; i < count; ++i) {
        auto const health{blue_health != -1 && teams[i] == ioj::sim::Team::Blue
                              ? blue_health
                              : data.turrets.max_health};
        ioj::sim::tests::add_turret_spawn(data, locations[i], {}, teams[i], health, laser_damage);
    }
}
}

void run_worldless_turret_combat(ioj::sim::tests::SimulationFixture const& config,
                                 TurretCombatScenario const scenario) {
    static std::vector<ioj::sim::Vector3f> const locations{{{3160.f, -3200.f, 40.f}},
                                                           {{1710.f, -3200.f, 40.f}},
                                                           {{190.f, -3200.f, 40.f}},
                                                           {{-1050.f, -3200.f, 40.f}},
                                                           {{-2380.f, -3200.f, 40.f}},
                                                           {{-3550.f, -3200.f, 40.f}},
                                                           {{190.f, 2280.f, 40.f}}};
    static std::vector<ioj::sim::Team> const teams{ioj::sim::Team::Blue,
                                                   ioj::sim::Team::Blue,
                                                   ioj::sim::Team::Blue,
                                                   ioj::sim::Team::Blue,
                                                   ioj::sim::Team::Blue,
                                                   ioj::sim::Team::Blue,
                                                   ioj::sim::Team::Red};
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const damage{scenario == TurretCombatScenario::ZeroDamage ? 0 : data.turrets.laser.damage};
    add_worldless_turrets(
        data, locations, teams, damage, scenario == TurretCombatScenario::KillEnemy ? 100000 : -1);

    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    std::vector<std::int32_t> initial_healths{};
    auto const& registry{harness.get_registry()};
    auto const initial_count{registry.get_num_elements()};
    initial_healths.reserve(initial_count);
    for (std::int32_t i{}; i < initial_count; ++i) {
        initial_healths.push_back(registry.get_entity_data().healths[i]);
    }
    harness.timeline.finish_at(3.0);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(3.5),
                                 "Turret combat timeline completes");

    if (scenario == TurretCombatScenario::KillEnemy) {
        ioj::sim::tests::expect_equal(1, registry.count_kills(), "One turret is killed");
        ioj::sim::tests::expect_equal(6, registry.count_alive(), "Hero turrets remain alive");
        for (auto const target : harness.get_simulation().get_turrets().get_target_handles()) {
            ioj::sim::tests::expect_true(target.is_null(), "Targets clear after the enemy dies");
        }
        return;
    }

    ioj::sim::tests::expect_equal(
        initial_count, registry.count_alive(), "Zero-damage turrets remain alive");
    for (std::int32_t i{}; i < initial_count; ++i) {
        ioj::sim::tests::expect_equal(initial_healths[i],
                                      registry.get_entity_data().healths[i],
                                      "Zero-damage combat preserves health",
                                      i);
    }
}

void run_worldless_turret_line_of_sight_blocking(ioj::sim::tests::SimulationFixture const& config) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    std::vector<ioj::sim::Vector3f> const locations{{{-5000.f, 0.f, 0.f}}, {{5000.f, 0.f, 0.f}}};
    std::vector<ioj::sim::Team> const teams{ioj::sim::Team::Blue, ioj::sim::Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
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
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(4.5),
                                 "Turret line-of-sight timeline completes");
    ioj::sim::tests::expect_greater(spawn_count_before_blocker, 0, "Turrets fire before blocking");
    ioj::sim::tests::expect_equal(spawn_count_before_blocker,
                                  harness.get_simulation().get_lasers().get_number_spawned(),
                                  "Turrets stop firing after line of sight is blocked");
}

void run_worldless_turret_search_requires_line_of_sight(
    ioj::sim::tests::SimulationFixture const& config) {
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    std::vector<ioj::sim::Vector3f> const locations{
        {{-1000.f, 0.f, 0.f}}, {{1000.f, 0.f, 0.f}}, {{1000.f, 1000.f, 0.f}}};
    std::vector<ioj::sim::Team> const teams{
        ioj::sim::Team::Blue, ioj::sim::Team::Red, ioj::sim::Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    harness.get_simulation()
        .get_spatial_query_manager()
        .get_collision_system()
        .get_uniform_grid()
        .add_static_aabb({{-100.f, -300.f, -3000.f}}, {{100.f, 300.f, 3000.f}});
    harness.timeline.finish_at(1.0);
    ioj::sim::tests::expect_true(harness.run_until_timeline_finished(1.5),
                                 "Turret search timeline completes");
    auto const targets{harness.get_simulation().get_turrets().get_target_handles()};
    ioj::sim::tests::expect_equal(
        3, static_cast<std::int32_t>(targets.size()), "All turret targets are available");
    if (static_cast<std::int32_t>(targets.size()) != 3) {
        return;
    }
    ioj::sim::tests::expect_true(targets[0].is_valid(), "Blue turret selects a visible target");
    if (targets[0].is_valid()) {
        ioj::sim::tests::expect_distance_near(ioj::sim::Vector3f{{1000.f, 1000.f, 0.f}},
                                              harness.get_registry().get_location(targets[0]),
                                              1.f,
                                              "Blue turret skips the blocked enemy");
    }
}

}
