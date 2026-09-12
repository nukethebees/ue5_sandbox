#include "test_laser_lifecycle.h"

#include <sandbox/simulation/world_aabb_operations.h>
#include <SandboxTests/support/SoftTestAssertions.h>

#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestCollisionActor.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/combat/lasers/TestLasersSimulation.h>
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

#include <Components/BoxComponent.h>

namespace ml {
namespace {
constexpr double projectile_queue_time{0.05};
constexpr double collision_test_end_time{1.5};
constexpr double expiry_test_end_time{0.3};
constexpr int32 normal_target_health{100};
constexpr int32 low_target_health{15};
constexpr int32 projectile_damage{10};
constexpr float projectile_speed{12000.f};
constexpr float collision_max_distance{20000.f};
constexpr float miss_max_distance{500.f};
}

void run_worldless_laser_lifecycle(FAutomationTestBase& test,
                                   FSoftTestAssertions& checks,
                                   USpaceGameLevelConfig const& config,
                                   ELaserLifecycleScenario const scenario) {
    auto data{make_worldless_simulation_test_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
    add_worldless_capital_spawn(data,
                                FVector3f{-4000.f, 0.f, 0.f},
                                ETestTeam::Blue,
                                INDEX_NONE,
                                60.f,
                                60.f,
                                normal_target_health);
    add_worldless_capital_spawn(data,
                                FVector3f{4000.f, 0.f, 0.f},
                                ETestTeam::Red,
                                INDEX_NONE,
                                60.f,
                                60.f,
                                scenario == ELaserLifecycleScenario::SimultaneousLethalHits
                                    ? low_target_health
                                    : normal_target_health);
    if (scenario == ELaserLifecycleScenario::WorldBlocker) {
        data.static_bounds.add_defaulted(1);
        simulation::collision::set(
            data.static_bounds, 0, {-1100.f, -100.f, -100.f}, {-900.f, 100.f, 100.f});
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto& lasers{harness.get_simulation().get_lasers()};
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    auto const shooter{capitals.get_handle(0)};
    auto const target{capitals.get_handle(1)};
    auto const initial_target_health{scenario == ELaserLifecycleScenario::SimultaneousLethalHits
                                         ? low_target_health
                                         : normal_target_health};

    struct Sample {
        int32 active_lasers{};
        int32 total_spawned{};
        int32 target_health{};
        int32 alive_entities{};
        int32 kills{};
    };
    TimeSeriesData<Sample> samples;
    harness.on_end_tick = [&](FLevelSimulation&) {
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
        auto const shooter_radius{harness.get_registry().get_entity_data().radii[shooter.index]};
        auto const target_direction{(target_location - shooter_location).GetSafeNormal()};
        auto start{shooter_location + target_direction * (shooter_radius + 100.f)};
        auto fire_direction{target_direction};
        if (scenario == ELaserLifecycleScenario::Miss) {
            start = FVector3f{0.f, 0.f, 100000.f};
            fire_direction = FVector3f{0.f, 0.f, 1.f};
        } else if (scenario == ELaserLifecycleScenario::WorldBlocker) {
            start = FVector3f{-2000.f, 0.f, 0.f};
            fire_direction = FVector3f{1.f, 0.f, 0.f};
        }

        auto const count{scenario == ELaserLifecycleScenario::SimultaneousLethalHits ? 2 : 1};
        test_lasers::SpawnRequests requests;
        requests.add_uninitialised(count);
        for (int32 i{}; i < count; ++i) {
            requests.locations.set(i, start);
            ml::assign(requests.rotations, i, fire_direction.Rotation());
            requests.base_velocities.set(i, FVector3f::ZeroVector);
            requests.damages[i] = projectile_damage;
            requests.speeds[i] = projectile_speed;
            requests.max_distances[i] = scenario == ELaserLifecycleScenario::Miss
                                          ? miss_max_distance
                                          : collision_max_distance;
            requests.instigator_handles[i] = shooter;
            requests.sources[i] =
                ml::make_laser_source(ETestTeam::White, ETestEntityType::TubeSpinner);
        }
        lasers.queue_laser_spawns(requests);
    });
    auto const end_time{scenario == ELaserLifecycleScenario::Miss ? expiry_test_end_time
                                                                  : collision_test_end_time};
    harness.timeline.finish_at(end_time);
    test.TestTrue(TEXT("Laser lifecycle timeline completes"),
                  harness.run_until_timeline_finished(end_time + 0.5));
    checks.is_true(!samples.is_empty(), TEXT("Laser lifecycle samples are recorded"));
    if (samples.is_empty()) {
        return;
    }

    auto const expected_spawn_count{
        scenario == ELaserLifecycleScenario::SimultaneousLethalHits ? 2 : 1};
    auto observed_committed_projectile{false};
    auto damage_was_delayed{false};
    for (auto const& sample : samples.values()) {
        if (sample.active_lasers == expected_spawn_count) {
            observed_committed_projectile = true;
            damage_was_delayed |= sample.target_health == initial_target_health;
        }
    }
    checks.is_true(observed_committed_projectile, TEXT("Queued projectile becomes active"));
    checks.is_true(damage_was_delayed, TEXT("Projectile commit precedes collision damage"));
    auto const& final{samples.last_value()};
    checks.are_equal(expected_spawn_count, final.total_spawned, TEXT("Projectile count spawned"));
    checks.are_equal(0, final.active_lasers, TEXT("No active projectiles remain"));
    if (scenario == ELaserLifecycleScenario::Hit) {
        checks.are_equal(normal_target_health - projectile_damage,
                         final.target_health,
                         TEXT("Projectile applies damage once"));
        checks.are_equal(2, final.alive_entities, TEXT("Nonlethal hit preserves both entities"));
    } else if (scenario == ELaserLifecycleScenario::SimultaneousLethalHits) {
        checks.is_true(final.target_health <= 0, TEXT("Simultaneous hits are lethal"));
        checks.are_equal(1, final.alive_entities, TEXT("Target is removed once"));
        checks.are_equal(1, final.kills, TEXT("One kill is recorded"));
    } else {
        checks.are_equal(normal_target_health,
                         final.target_health,
                         TEXT("Non-entity termination preserves health"));
        checks.are_equal(2, final.alive_entities, TEXT("Both entities remain alive"));
        checks.are_equal(0, final.kills, TEXT("No kill is recorded"));
    }
}

}
