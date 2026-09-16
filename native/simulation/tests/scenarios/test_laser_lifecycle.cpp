#include "test_laser_lifecycle.h"
#include <ioj/sim/testing/level_sim_test_access.h>
#include "../support/simulation_test_support.h"

#include <ioj/sim/world_aabb_operations.h>

#include <ioj/sim/entity_registry.h>
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

TEST(NativeSimulation, LaserSpawnRequestRowOperationsKeepColumnsPaired) {
    lasers::SpawnRequests requests{};
    RegistryEntityHandle const instigator{7, 3};
    LaserSource const source{Team::Green, EntityType::CapitalShip};

    auto expect_row = [&](std::int32_t const index, float const offset) {
        EXPECT_FLOAT_EQ(requests.locations.xs[index], offset + 1.f);
        EXPECT_FLOAT_EQ(requests.locations.ys[index], offset + 2.f);
        EXPECT_FLOAT_EQ(requests.locations.zs[index], offset + 3.f);
        EXPECT_FLOAT_EQ(requests.rotations.pitches[index], offset + 4.f);
        EXPECT_FLOAT_EQ(requests.rotations.yaws[index], offset + 5.f);
        EXPECT_FLOAT_EQ(requests.rotations.rolls[index], offset + 6.f);
        EXPECT_FLOAT_EQ(requests.base_velocities.xs[index], offset + 7.f);
        EXPECT_FLOAT_EQ(requests.base_velocities.ys[index], offset + 8.f);
        EXPECT_FLOAT_EQ(requests.base_velocities.zs[index], offset + 9.f);
        EXPECT_EQ(requests.damages[index], static_cast<std::int32_t>(offset) + 10);
        EXPECT_FLOAT_EQ(requests.speeds[index], offset + 11.f);
        EXPECT_FLOAT_EQ(requests.max_distances[index], offset + 12.f);
        EXPECT_EQ(requests.instigator_handles[index], instigator);
        EXPECT_EQ(requests.sources[index], source);
    };

    EXPECT_EQ(requests.add({{1.f, 2.f, 3.f}},
                           {4.f, 5.f, 6.f},
                           {{7.f, 8.f, 9.f}},
                           10,
                           11.f,
                           12.f,
                           instigator,
                           source),
              0);
    EXPECT_EQ(requests.add({}, {}, {}, 0, 0.f, 0.f, {}, {}), 1);
    ASSERT_EQ(requests.num(), 2);
    requests.validate_array_sizes();
    expect_row(0, 0.f);

    requests.set(0,
                 {{21.f, 22.f, 23.f}},
                 {24.f, 25.f, 26.f},
                 {{27.f, 28.f, 29.f}},
                 30,
                 31.f,
                 32.f,
                 instigator,
                 source);
    EXPECT_FLOAT_EQ(requests.locations.xs[1], 0.f);
    EXPECT_EQ(requests.damages[1], 0);

    requests.get_view(1, 1).set(0,
                                {{1.f, 2.f, 3.f}},
                                {4.f, 5.f, 6.f},
                                {{7.f, 8.f, 9.f}},
                                10,
                                11.f,
                                12.f,
                                instigator,
                                source);
    requests.validate_array_sizes();

    expect_row(0, 20.f);
    expect_row(1, 0.f);
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
    auto const shooter{capitals.get_handle(0)};
    auto const target{capitals.get_handle(1)};

    struct Sample {
        std::int32_t active_lasers{};
        std::int32_t total_spawned{};
        std::int32_t target_health{};
        std::int32_t alive_entities{};
        std::int32_t kills{};
    };
    ml::TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](LevelSim&) {
        samples.add(harness.get_time(),
                    Sample{lasers.get_num_instances(),
                           lasers.get_number_spawned(),
                           harness.get_registry().get_health(target),
                           harness.get_registry().count_alive(),
                           harness.get_registry().count_kills()});
    };
    harness.timeline.at(projectile_queue_time, [&] {
        auto const shooter_location{harness.get_registry().get_location(shooter)};
        auto const target_location{harness.get_registry().get_location(target)};
        auto const shooter_radius{
            harness.get_simulation().get_spatial_query_manager().get_entity_type_radius(
                harness.get_registry().get_entity_type(shooter))};
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
        lasers::SpawnRequests requests;
        requests.add_uninitialised(count);
        for (std::int32_t i{}; i < count; ++i) {
            requests.locations.set(i, start);
            requests.rotations.set(i, direction_to_rotation(fire_direction));
            requests.base_velocities.set(i, Vector3f{});
            requests.damages[i] = projectile_damage;
            requests.speeds[i] = projectile_speed;
            requests.max_distances[i] = scenario == LaserLifecycleScenario::Miss
                                          ? miss_max_distance
                                          : collision_max_distance;
            requests.instigator_handles[i] = shooter;
            requests.sources[i] = LaserSource{Team::White, EntityType::TubeSpinner};
        }
        LevelSimTestAccess::queue_laser_spawns(harness.get_simulation(), requests.get_const_view());
    });
    auto const end_time{scenario == LaserLifecycleScenario::Miss ? expiry_test_end_time
                                                                 : collision_test_end_time};
    harness.timeline.finish_at(end_time);
    tests::expect_true(harness.run_until_timeline_finished(end_time + 0.5),
                       "Laser lifecycle timeline completes");
    tests::expect_true(!samples.is_empty(), "Laser lifecycle samples are recorded");
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
    tests::expect_true(observed_committed_projectile,
                       "Queued projectile is committed in Preparation");
    auto const& final{samples.last_value()};
    tests::expect_equal(expected_spawn_count, final.total_spawned, "Projectile count spawned");
    tests::expect_equal(0, final.active_lasers, "No active projectiles remain");
    if (scenario == LaserLifecycleScenario::Hit) {
        tests::expect_equal(normal_target_health - projectile_damage,
                            final.target_health,
                            "Projectile applies damage once");
        tests::expect_equal(2, final.alive_entities, "Nonlethal hit preserves both entities");
    } else if (scenario == LaserLifecycleScenario::SimultaneousLethalHits) {
        tests::expect_true(final.target_health <= 0, "Simultaneous hits are lethal");
        tests::expect_equal(1, final.alive_entities, "Target is removed once");
        tests::expect_equal(1, final.kills, "One kill is recorded");
    } else {
        tests::expect_equal(
            normal_target_health, final.target_health, "Non-entity termination preserves health");
        tests::expect_equal(2, final.alive_entities, "Both entities remain alive");
        tests::expect_equal(0, final.kills, "No kill is recorded");
    }
}

}
