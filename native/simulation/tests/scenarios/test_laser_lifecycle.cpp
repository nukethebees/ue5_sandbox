#include "../support/simulation_test_support.h"
#include <ioj/column_math.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/laser_spawns.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <memory_resource>

namespace ioj::sim {
namespace laser_lifecycle_test {
auto make_fixture(std::int32_t const target_health = 100) -> tests::SimulationScenario {
    tests::SimulationScenario data{tests::make_simulation_data(tests::make_fixture())};
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    tests::add_capital_spawn(
        data, {-4000.f, 0.f, 0.f}, Team::Blue, 60.f, 60.f, settings::PositiveHealth{Health{100}});
    tests::add_capital_spawn(data,
                             {4000.f, 0.f, 0.f},
                             Team::Red,
                             60.f,
                             60.f,
                             settings::PositiveHealth::make(Health{target_health}));
    return data;
}

void advance_to_projectile_queue_time(LevelSim& simulation) {
    simulation.start();
    simulation.advance(0.05);
}

void queue_projectiles(LevelSim& simulation,
                       Vector3f const start,
                       Vector3f const direction,
                       float const max_distance = 20000.f,
                       std::int32_t const count = 1) {
    auto const shooter{simulation.capital_ships().get_entities().entity_ids()[0]};
    lasers::LaserSpawnRequests requests;
    for (std::int32_t i{}; i < count; ++i) {
        tests::add_laser_spawn(requests,
                               start,
                               direction_to_rotation(direction),
                               {},
                               10,
                               12000.f,
                               max_distance,
                               shooter,
                               LaserSource{Team::White, EntityType::TubeSpinner});
    }
    LevelSimTestAccess::queue_laser_spawns(simulation, requests.const_view());
}

void queue_targeted_projectiles(LevelSim& simulation, std::int32_t const count = 1) {
    auto const ids{simulation.capital_ships().get_entities().entity_ids()};
    auto const shooter_location{observe_entity(simulation, ids[0])->location};
    auto const target_location{observe_entity(simulation, ids[1])->location};
    auto const radius{
        simulation.spatial_query_manager().entity_type_radius(EntityType::CapitalShip)};
    auto const direction{normalized(target_location - shooter_location)};
    queue_projectiles(
        simulation, shooter_location + direction * (radius + 100.f), direction, 20000.f, count);
}
}

TEST(WorldlessSpaceGameSimulation, LasersQueuedSpawnHitsOnLaterTick) {
    LevelSim simulation{laser_lifecycle_test::make_fixture().compile(),
                        *std::pmr::new_delete_resource()};
    auto const target{simulation.capital_ships().get_entities().entity_ids()[1]};
    laser_lifecycle_test::advance_to_projectile_queue_time(simulation);
    laser_lifecycle_test::queue_targeted_projectiles(simulation);
    simulation.advance(1.5);

    EXPECT_EQ(simulation.lasers().get_number_spawned(), 1u);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);
    EXPECT_EQ(observe_entity(simulation, target)->health.value(), 90)
        << "Projectile applies damage once";
    EXPECT_EQ(simulation.entity_ledger().count_alive(), 2);
}

TEST(WorldlessSpaceGameSimulation, LasersSimultaneousHitsCauseOneDeath) {
    LevelSim simulation{laser_lifecycle_test::make_fixture(15).compile(),
                        *std::pmr::new_delete_resource()};
    auto const ids{simulation.capital_ships().get_entities().entity_ids()};
    auto const shooter{ids[0]};
    auto const target{ids[1]};
    laser_lifecycle_test::advance_to_projectile_queue_time(simulation);
    laser_lifecycle_test::queue_targeted_projectiles(simulation, 2);
    simulation.advance(1.5);

    EXPECT_EQ(simulation.lasers().get_number_spawned(), 2u);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);
    EXPECT_FALSE(entity_is_alive(simulation, target));
    EXPECT_EQ(simulation.entity_ledger().count_alive(), 1);
    EXPECT_EQ(simulation.entity_ledger().kills(shooter), 1) << "One kill is recorded";
}

TEST(WorldlessSpaceGameSimulation, LasersMissExpiresWithoutDamage) {
    LevelSim simulation{laser_lifecycle_test::make_fixture().compile(),
                        *std::pmr::new_delete_resource()};
    auto const ids{simulation.capital_ships().get_entities().entity_ids()};
    auto const shooter{ids[0]};
    auto const target{ids[1]};
    laser_lifecycle_test::advance_to_projectile_queue_time(simulation);
    laser_lifecycle_test::queue_projectiles(
        simulation, {0.f, 0.f, 100000.f}, {0.f, 0.f, 1.f}, 500.f);
    simulation.advance(0.3);

    EXPECT_EQ(simulation.lasers().get_number_spawned(), 1u);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);
    EXPECT_EQ(observe_entity(simulation, target)->health.value(), 100);
    EXPECT_EQ(simulation.entity_ledger().count_alive(), 2);
    EXPECT_EQ(simulation.entity_ledger().kills(shooter), 0);
}

TEST(WorldlessSpaceGameSimulation, LasersWorldBlockerConsumesProjectileWithoutEntityDamage) {
    auto data{laser_lifecycle_test::make_fixture()};
    data.runtime.collision.static_bounds.add({-1100.f, -100.f, -100.f}, {-900.f, 100.f, 100.f});
    LevelSim simulation{std::move(data).compile(), *std::pmr::new_delete_resource()};
    auto const ids{simulation.capital_ships().get_entities().entity_ids()};
    auto const shooter{ids[0]};
    auto const target{ids[1]};
    laser_lifecycle_test::advance_to_projectile_queue_time(simulation);
    laser_lifecycle_test::queue_projectiles(simulation, {-2000.f, 0.f, 0.f}, {1.f, 0.f, 0.f});
    simulation.advance(1.5);

    EXPECT_EQ(simulation.lasers().get_number_spawned(), 1u);
    EXPECT_EQ(simulation.lasers().num_instances(), 0);
    EXPECT_EQ(observe_entity(simulation, target)->health.value(), 100);
    EXPECT_EQ(simulation.entity_ledger().count_alive(), 2);
    EXPECT_EQ(simulation.entity_ledger().kills(shooter), 0);
}
}
