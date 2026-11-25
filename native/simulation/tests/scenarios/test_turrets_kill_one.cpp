#include "../support/simulation_test_support.h"
#include <ioj/sim/collision/world_aabbs.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/level_sim_test_access.h>
#include <ioj/sim/turrets/sim.h>

#include <memory_resource>

namespace ioj::sim {
namespace {
void add_worldless_turrets(tests::SimulationScenario& data,
                           std::span<Vector3f const> const locations,
                           std::span<Team const> const teams,
                           std::int32_t const laser_damage) {
    assert(static_cast<std::int32_t>(locations.size()) == static_cast<std::int32_t>(teams.size()));
    auto const count{static_cast<std::int32_t>(locations.size())};
    for (std::int32_t i{}; i < count; ++i) {
        tests::add_turret_spawn(data,
                                locations[i],
                                {},
                                teams[i],
                                {},
                                ioj::NonNegative<std::int32_t>::make(laser_damage));
    }
}
}

auto make_turret_duel(std::int32_t const laser_damage) -> LevelSimInitData {
    tests::SimulationScenario data{tests::make_simulation_data(tests::make_fixture())};
    auto const damage{NonNegative<std::int32_t>::make(laser_damage)};
    tests::add_turret_spawn(data,
                            {190.f, -3200.f, 40.f},
                            {},
                            Team::Blue,
                            settings::PositiveHealth{Health{100000}},
                            damage);
    tests::add_turret_spawn(data, {190.f, 2280.f, 40.f}, {}, Team::Red, {}, damage);
    return std::move(data).compile();
}

TEST(WorldlessSpaceGameSimulation, TurretsLineOfSightBlocking) {
    auto const config{tests::make_fixture()};
    tests::SimulationScenario data{tests::make_simulation_data(config)};
    std::vector<Vector3f> const locations{{{-5000.f, 0.f, 0.f}}, {{5000.f, 0.f, 0.f}}};
    std::vector<Team> const teams{Team::Blue, Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    tests::WorldlessSimulationTest harness{std::move(data).compile()};

    auto& simulation{harness.simulation()};
    simulation.start();
    simulation.advance(2.0);
    auto const spawn_count_before_blocker{simulation.lasers().get_number_spawned()};
    collision::WorldAABBs blockers;
    blockers.add({{-500.f, -3000.f, -3000.f}}, {{500.f, 3000.f, 3000.f}});
    LevelSimTestAccess::spatial_queries(simulation).set_static_collision(std::move(blockers));
    simulation.advance(2.0);
    EXPECT_GT(spawn_count_before_blocker, 0) << "Turrets fire before blocking";
    EXPECT_EQ(spawn_count_before_blocker, harness.simulation().lasers().get_number_spawned())
        << "Turrets stop firing after line of sight is blocked";
}

TEST(WorldlessSpaceGameSimulation, TurretsSearchRequiresLineOfSight) {
    auto const config{tests::make_fixture()};
    tests::SimulationScenario data{tests::make_simulation_data(config)};
    std::vector<Vector3f> const locations{
        {{-1000.f, 0.f, 0.f}}, {{1000.f, 0.f, 0.f}}, {{1000.f, 1000.f, 0.f}}};
    std::vector<Team> const teams{Team::Blue, Team::Red, Team::Red};
    add_worldless_turrets(data, locations, teams, 0);
    data.runtime.collision.static_bounds.add({{-100.f, -300.f, -3000.f}}, {{100.f, 300.f, 3000.f}});
    tests::WorldlessSimulationTest harness{std::move(data).compile()};

    harness.simulation().start();
    harness.simulation().advance(1.0);
    auto const targets{harness.simulation().turrets().get_entities().target_ids()};
    EXPECT_EQ(3, static_cast<std::int32_t>(targets.size())) << "All turret targets are available";
    if (static_cast<std::int32_t>(targets.size()) != 3) {
        return;
    }

    EXPECT_TRUE(targets[0].is_valid()) << "Blue turret selects a visible target";
    if (targets[0].is_valid()) {
        EXPECT_LE(ioj::size(Vector3f{{1000.f, 1000.f, 0.f}} -
                            observe_live_entity(harness.simulation(), targets[0])->location),
                  1.f)
            << "Blue turret skips the blocked enemy";
    }
}

TEST(WorldlessSpaceGameSimulation, TurretsKillEnemy) {
    LevelSim simulation{make_turret_duel(100), *std::pmr::new_delete_resource()};
    simulation.start();
    simulation.advance(3.0);

    auto const survivors{simulation.turrets().get_entities()};
    ASSERT_EQ(survivors.num(), 1);
    EXPECT_EQ(survivors.teams()[0], Team::Blue);
    EXPECT_EQ(simulation.entity_ledger().count_kills(Team::Blue), 1);
}

TEST(WorldlessSpaceGameSimulation, TurretsClearTargetsAfterEnemyDies) {
    LevelSim simulation{make_turret_duel(100), *std::pmr::new_delete_resource()};
    simulation.start();
    simulation.advance(3.0);

    ASSERT_EQ(simulation.entity_ledger().count_kills(Team::Blue), 1);
    auto const survivors{simulation.turrets().get_entities()};
    ASSERT_EQ(survivors.num(), 1);
    EXPECT_FALSE(survivors.target_ids()[0].is_valid());
}

TEST(WorldlessSpaceGameSimulation, TurretsZeroDamage) {
    LevelSim simulation{make_turret_duel(0), *std::pmr::new_delete_resource()};
    auto const& turrets{simulation.turrets()};
    auto const ids{turrets.get_entities().entity_ids()};
    auto const blue{ids[0]};
    auto const red{ids[1]};
    auto const blue_health{
        simulation.entity_tables().health.const_view<EntityType::Turret>().health(blue)};
    auto const red_health{
        simulation.entity_tables().health.const_view<EntityType::Turret>().health(red)};
    simulation.start();
    simulation.advance(3.0);

    ASSERT_EQ(turrets.num_instances(), 2);
    EXPECT_EQ(simulation.entity_tables().health.const_view<EntityType::Turret>().health(blue),
              blue_health);
    EXPECT_EQ(simulation.entity_tables().health.const_view<EntityType::Turret>().health(red),
              red_health);
}

}
