#include "test_laser_lifecycle.h"
#include <ioj/sim/column_math.h>
#include <ioj/sim/testing/laser_spawns.h>
#include <ioj/sim/testing/level_sim_test_access.h>
#include "../support/simulation_test_support.h"

#include <ioj/sim/world_aabb_operations.h>

#include <ioj/sim/lasers/sim.h>

namespace ioj::sim {
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

void run_worldless_laser_lifecycle(tests::SimulationFixture const& config,
                                   LaserLifecycleScenario const scenario) {
    auto data{tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    tests::add_capital_spawn(
        data, Vector3f{{-4000.f, 0.f, 0.f}}, Team::Blue, -1, 60.f, 60.f, normal_target_health);
    tests::add_capital_spawn(data,
                             Vector3f{{4000.f, 0.f, 0.f}},
                             Team::Red,
                             -1,
                             60.f,
                             60.f,
                             scenario == LaserLifecycleScenario::SimultaneousLethalHits
                                 ? low_target_health
                                 : normal_target_health);
    if (scenario == LaserLifecycleScenario::WorldBlocker) {
        data.static_bounds.add_defaulted(1);
        collision::set(
            data.static_bounds, 0, {{-1100.f, -100.f, -100.f}}, {{-900.f, 100.f, 100.f}});
    }

    tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& lasers{harness.get_simulation().get_lasers()};
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const shooter{capitals.get_id(0)};
    auto const target{capitals.get_id(1)};

    struct Sample {
        std::int32_t active_lasers{};
        std::int32_t total_spawned{};
        std::int32_t target_health{};
        std::int32_t alive_entities{};
        std::int32_t kills{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim& simulation) {
        auto const target_state{simulation.get_agent_accessor().read(target)};
        samples.add(harness.get_time(),
                    Sample{lasers.get_num_instances(),
                           lasers.get_number_spawned(),
                           target_state ? target_state->health : 0,
                           harness.get_ledger().count_alive(),
                           harness.get_ledger().count_kills()});
    };
    harness.timeline.at(projectile_queue_time, [&] {
        auto const shooter_location{
            harness.get_simulation().get_agent_accessor().read(shooter)->location};
        auto const target_location{
            harness.get_simulation().get_agent_accessor().read(target)->location};
        auto const shooter_radius{
            harness.get_simulation().get_spatial_query_manager().get_entity_type_radius(
                shooter.entity_type())};
        auto const target_direction{HMM_NormV3(target_location - shooter_location)};
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
        lasers::SingleAllocationLaserSpawnRequests requests;
        requests.add_uninitialised(count);
        for (std::int32_t i{}; i < count; ++i) {
            set_vector(requests.get_view().view_locations(), i, start);
            set_rotation(
                requests.get_view().view_rotations(), i, direction_to_rotation(fire_direction));
            set_vector(requests.get_view().view_base_velocities(), i, Vector3f{});
            requests.get_view().damages()[i] = projectile_damage;
            requests.get_view().speeds()[i] = projectile_speed;
            requests.get_view().max_distances()[i] = scenario == LaserLifecycleScenario::Miss
                                                       ? miss_max_distance
                                                       : collision_max_distance;
            requests.get_view().instigator_ids()[i] = shooter;
            requests.get_view().sources()[i] = LaserSource{Team::White, EntityType::TubeSpinner};
        }
        LevelSimTestAccess::queue_laser_spawns(harness.get_simulation(), requests.get_const_view());
    });
    auto const end_time{scenario == LaserLifecycleScenario::Miss ? expiry_test_end_time
                                                                 : collision_test_end_time};
    harness.timeline.finish_at(end_time);
    EXPECT_TRUE(harness.run_until_timeline_finished(end_time + 0.5))
        << "Laser lifecycle timeline completes";
    EXPECT_TRUE(!samples.is_empty()) << "Laser lifecycle samples are recorded";
    if (samples.is_empty()) {
        return;
    }

    auto const expected_spawn_count{scenario == LaserLifecycleScenario::SimultaneousLethalHits ? 2
                                                                                               : 1};
    auto observed_committed_projectile{false};
    for (auto const& sample : samples.values()) {
        if (sample.total_spawned == expected_spawn_count) {
            observed_committed_projectile = true;
        }
    }
    EXPECT_TRUE(observed_committed_projectile) << "Queued projectile is committed in Preparation";
    auto const& final{samples.last_value()};
    EXPECT_EQ(expected_spawn_count, final.total_spawned) << "Projectile count spawned";
    EXPECT_EQ(0, final.active_lasers) << "No active projectiles remain";
    if (scenario == LaserLifecycleScenario::Hit) {
        EXPECT_EQ(normal_target_health - projectile_damage, final.target_health)
            << "Projectile applies damage once";
        EXPECT_EQ(2, final.alive_entities) << "Nonlethal hit preserves both entities";
    } else if (scenario == LaserLifecycleScenario::SimultaneousLethalHits) {
        EXPECT_TRUE(final.target_health <= 0) << "Simultaneous hits are lethal";
        EXPECT_EQ(1, final.alive_entities) << "Target is removed once";
        EXPECT_EQ(1, final.kills) << "One kill is recorded";
    } else {
        EXPECT_EQ(normal_target_health, final.target_health)
            << "Non-entity termination preserves health";
        EXPECT_EQ(2, final.alive_entities) << "Both entities remain alive";
        EXPECT_EQ(0, final.kills) << "No kill is recorded";
    }
}

}
