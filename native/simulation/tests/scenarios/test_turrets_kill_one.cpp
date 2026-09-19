#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/turrets/sim.h>
#include "../support/simulation_test_support.h"

#include "test_turrets_kill_one.h"

namespace ioj::sim {
namespace {
void add_worldless_turrets(LevelSimInitData& data,
                           std::span<Vector3f const> const locations,
                           std::span<Team const> const teams,
                           std::int32_t const laser_damage,
                           std::int32_t const blue_health = -1) {
    assert(static_cast<std::int32_t>(locations.size()) == static_cast<std::int32_t>(teams.size()));
    auto const count{static_cast<std::int32_t>(locations.size())};
    for (std::int32_t i{}; i < count; ++i) {
        auto const health{blue_health != -1 && teams[i] == Team::Blue ? blue_health
                                                                      : data.turrets.max_health};
        tests::add_turret_spawn(data, locations[i], {}, teams[i], health, laser_damage);
    }
}
}

void run_worldless_turret_combat(tests::SimulationFixture const& config,
                                 TurretCombatScenario const scenario) {
    static std::vector<Vector3f> const locations{{{3160.f, -3200.f, 40.f}},
                                                 {{1710.f, -3200.f, 40.f}},
                                                 {{190.f, -3200.f, 40.f}},
                                                 {{-1050.f, -3200.f, 40.f}},
                                                 {{-2380.f, -3200.f, 40.f}},
                                                 {{-3550.f, -3200.f, 40.f}},
                                                 {{190.f, 2280.f, 40.f}}};
    static std::vector<Team> const teams{
        Team::Blue, Team::Blue, Team::Blue, Team::Blue, Team::Blue, Team::Blue, Team::Red};
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    auto const damage{scenario == TurretCombatScenario::ZeroDamage ? 0 : data.turrets.laser.damage};
    add_worldless_turrets(
        data, locations, teams, damage, scenario == TurretCombatScenario::KillEnemy ? 100000 : -1);

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    std::vector<std::int32_t> initial_healths{};
    auto const initial_view{harness.get_simulation().get_turrets().get_read_view()};
    auto const initial_entities{initial_view.entities};
    auto const initial_count{initial_entities.num()};
    initial_healths.reserve(initial_count);
    for (std::int32_t i{}; i < initial_count; ++i) {
        initial_healths.push_back(initial_view.healths.health(i));
    }
    harness.timeline.finish_at(3.0);
    tests::expect_true(harness.run_until_timeline_finished(3.5),
                       "Turret combat timeline completes");

    if (scenario == TurretCombatScenario::KillEnemy) {
        tests::expect_equal(1, harness.get_ledger().count_kills(), "One turret is killed");
        tests::expect_equal(6, harness.get_ledger().count_alive(), "Hero turrets remain alive");
        for (auto const target : harness.get_simulation().get_turrets().get_target_ids()) {
            tests::expect_true(!target.is_valid(), "Targets clear after the enemy dies");
        }
        return;
    }

    tests::expect_equal(
        initial_count, harness.get_ledger().count_alive(), "Zero-damage turrets remain alive");
    auto const final_view{harness.get_simulation().get_turrets().get_read_view()};
    for (std::int32_t i{}; i < initial_count; ++i) {
        tests::expect_equal(initial_healths[i],
                            final_view.healths.health(i),
                            "Zero-damage combat preserves health",
                            i);
    }
}

void run_worldless_turret_line_of_sight_blocking(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    std::vector<Vector3f> const locations{{{-5000.f, 0.f, 0.f}}, {{5000.f, 0.f, 0.f}}};
    std::vector<Team> const teams{Team::Blue, Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    std::int32_t spawn_count_before_blocker{-1};
    harness.timeline
        .at(2.0,
            [&] {
                spawn_count_before_blocker =
                    harness.get_simulation().get_lasers().get_number_spawned();
                harness.get_simulation().add_static_collision_aabb({{-500.f, -3000.f, -3000.f}},
                                                                   {{500.f, 3000.f, 3000.f}});
            })
        .finish_at(4.0);
    tests::expect_true(harness.run_until_timeline_finished(4.5),
                       "Turret line-of-sight timeline completes");
    tests::expect_greater(spawn_count_before_blocker, 0, "Turrets fire before blocking");
    tests::expect_equal(spawn_count_before_blocker,
                        harness.get_simulation().get_lasers().get_number_spawned(),
                        "Turrets stop firing after line of sight is blocked");
}

void run_worldless_turret_search_requires_line_of_sight(tests::SimulationFixture const& config) {
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    std::vector<Vector3f> const locations{
        {{-1000.f, 0.f, 0.f}}, {{1000.f, 0.f, 0.f}}, {{1000.f, 1000.f, 0.f}}};
    std::vector<Team> const teams{Team::Blue, Team::Red, Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    harness.get_simulation().add_static_collision_aabb({{-100.f, -300.f, -3000.f}},
                                                       {{100.f, 300.f, 3000.f}});
    harness.timeline.finish_at(1.0);
    tests::expect_true(harness.run_until_timeline_finished(1.5),
                       "Turret search timeline completes");
    auto const targets{harness.get_simulation().get_turrets().get_target_ids()};
    tests::expect_equal(
        3, static_cast<std::int32_t>(targets.size()), "All turret targets are available");
    if (static_cast<std::int32_t>(targets.size()) != 3) {
        return;
    }
    tests::expect_true(targets[0].is_valid(), "Blue turret selects a visible target");
    if (targets[0].is_valid()) {
        tests::expect_distance_near(
            Vector3f{{1000.f, 1000.f, 0.f}},
            harness.get_simulation().get_agent_accessor().read_alive(targets[0])->location,
            1.f,
            "Blue turret skips the blocked enemy");
    }
}

}
