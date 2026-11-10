#include "../support/simulation_test_support.h"
#include <ioj/column_math.h>
#include <ioj/sim/collision/world_aabbs.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/testing/entity_observations.h>
#include <ioj/sim/testing/laser_spawns.h>
#include <ioj/sim/testing/level_sim_test_access.h>

namespace ioj::sim {
enum class LaserLifecycleScenario : std::uint8_t {
    Hit,
    SimultaneousLethalHits,
    Miss,
    WorldBlocker
};

namespace {
constexpr double projectile_queue_time{0.05};
constexpr double collision_test_end_time{1.5};
constexpr double expiry_test_end_time{0.3};
constexpr std::int32_t normal_target_health{100};
constexpr std::int32_t low_target_health{15};
constexpr std::int32_t projectile_damage{10};
constexpr float projectile_speed{12000.f};
constexpr float collision_max_distance{20000.f};
constexpr float miss_max_distance{500.f};
}

void run_worldless_laser_lifecycle(LaserLifecycleScenario const scenario) {
    tests::SimulationScenario data{tests::make_simulation_data(tests::make_fixture())};
    data.runtime.capital_ships.fighter_spawn_slots = 0;
    data.runtime.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    tests::add_capital_spawn(
        data,
        Vector3f{{-4000.f, 0.f, 0.f}},
        Team::Blue,
        60.f,
        60.f,
        ioj::settings::PositiveHealth::make(ioj::sim::Health{normal_target_health}));
    tests::add_capital_spawn(
        data,
        Vector3f{{4000.f, 0.f, 0.f}},
        Team::Red,
        60.f,
        60.f,
        ioj::settings::PositiveHealth::make(ioj::sim::Health{
            scenario == LaserLifecycleScenario::SimultaneousLethalHits ? low_target_health
                                                                       : normal_target_health}));
    if (scenario == LaserLifecycleScenario::WorldBlocker) {
        data.runtime.collision.static_bounds.add_defaulted(1);
        data.runtime.collision.static_bounds.set(
            0, {{-1100.f, -100.f, -100.f}}, {{-900.f, 100.f, 100.f}});
    }

    tests::WorldlessSimulationTest harness{std::move(data).compile()};

    auto const& lasers{harness.simulation().lasers()};
    auto const& capitals{harness.simulation().capital_ships()};
    auto const shooter{capitals.get_entities().entity_ids()[0]};
    auto const target{capitals.get_entities().entity_ids()[1]};

    harness.timeline.at(projectile_queue_time, [&] {
        auto const shooter_location{observe_entity(harness.simulation(), shooter)->location};
        auto const target_location{observe_entity(harness.simulation(), target)->location};
        auto const shooter_radius{
            harness.simulation().spatial_query_manager().entity_type_radius(shooter.entity_type())};
        auto const target_direction{normalized(target_location - shooter_location)};
        auto start{shooter_location + target_direction * (shooter_radius + 100.f)};
        auto fire_direction{target_direction};
        if (scenario == LaserLifecycleScenario::Miss) {
            start = Vector3f{{0.f, 0.f, 100000.f}};
            fire_direction = Vector3f{{0.f, 0.f, 1.f}};
        } else if (scenario == LaserLifecycleScenario::WorldBlocker) {
            start = Vector3f{{-2000.f, 0.f, 0.f}};
            fire_direction = Vector3f{{1.f, 0.f, 0.f}};
        }

        auto const count{scenario == LaserLifecycleScenario::SimultaneousLethalHits ? 2 : 1};
        lasers::LaserSpawnRequests requests;
        for (std::int32_t i{}; i < count; ++i) {
            tests::add_laser_spawn(requests,
                                   start,
                                   direction_to_rotation(fire_direction),
                                   {},
                                   projectile_damage,
                                   projectile_speed,
                                   scenario == LaserLifecycleScenario::Miss
                                       ? miss_max_distance
                                       : collision_max_distance,
                                   shooter,
                                   LaserSource{Team::White, EntityType::TubeSpinner});
        }
        LevelSimTestAccess::queue_laser_spawns(harness.simulation(), requests.const_view());
    });
    auto const end_time{scenario == LaserLifecycleScenario::Miss ? expiry_test_end_time
                                                                 : collision_test_end_time};
    harness.timeline.finish_at(end_time);
    ASSERT_TRUE(harness.run_until_timeline_finished(end_time + 0.5))
        << "Laser lifecycle timeline completes";

    auto const expected_spawn_count{
        scenario == LaserLifecycleScenario::SimultaneousLethalHits ? 2u : 1u};
    EXPECT_EQ(expected_spawn_count, lasers.get_number_spawned()) << "Projectile count spawned";
    EXPECT_EQ(0, lasers.num_instances()) << "No active projectiles remain";
    auto const target_state{observe_entity(harness.simulation(), target)};
    auto const target_health{target_state ? target_state->health.value() : 0};
    auto const& ledger{harness.ledger()};
    if (scenario == LaserLifecycleScenario::Hit) {
        EXPECT_EQ(normal_target_health - projectile_damage, target_health)
            << "Projectile applies damage once";
        EXPECT_EQ(2, ledger.count_alive()) << "Nonlethal hit preserves both entities";
    } else if (scenario == LaserLifecycleScenario::SimultaneousLethalHits) {
        EXPECT_TRUE(target_health <= 0) << "Simultaneous hits are lethal";
        EXPECT_EQ(1, ledger.count_alive()) << "Target is removed once";
        EXPECT_EQ(1, ledger.kills(shooter)) << "One kill is recorded";
    } else {
        EXPECT_EQ(normal_target_health, target_health) << "Non-entity termination preserves health";
        EXPECT_EQ(2, ledger.count_alive()) << "Both entities remain alive";
        EXPECT_EQ(0, ledger.kills(shooter)) << "No kill is recorded";
    }
}

TEST(WorldlessSpaceGameSimulation, LasersQueuedSpawnHitsOnLaterTick) {
    run_worldless_laser_lifecycle(LaserLifecycleScenario::Hit);
}

TEST(WorldlessSpaceGameSimulation, LasersSimultaneousHitsCauseOneDeath) {
    run_worldless_laser_lifecycle(LaserLifecycleScenario::SimultaneousLethalHits);
}

TEST(WorldlessSpaceGameSimulation, LasersMissExpiresWithoutDamage) {
    run_worldless_laser_lifecycle(LaserLifecycleScenario::Miss);
}

TEST(WorldlessSpaceGameSimulation, LasersWorldBlockerConsumesProjectileWithoutEntityDamage) {
    run_worldless_laser_lifecycle(LaserLifecycleScenario::WorldBlocker);
}

}
