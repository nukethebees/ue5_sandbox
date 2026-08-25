#include "test_laser_lifecycle_scenario.h"

#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SpaceGame/combat/lasers/TestLasers.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCore/soa_rotator_utils.h>
#include <SandboxCore/soa_vector_utils.h>

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

FLaserLifecycleScenario::FLaserLifecycleScenario(FSimulationTestContext& context,
                                                 ELaserLifecycleScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FLaserLifecycleScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    if (driver.IsSet()) {
        driver->orchestrator.clear_end_tick_test_hook();
        driver->orchestrator.pause_simulation();
    }
}

void FLaserLifecycleScenario::spawn_fixture() {
    auto* const shooter{spawn_capital_proxy(context_.world,
                                            context_.config,
                                            checks,
                                            TEXT("laser_shooter"),
                                            FVector{-4000.f, 0.f, 0.f})};
    auto* const target{spawn_capital_proxy(
        context_.world, context_.config, checks, TEXT("laser_target"), FVector{4000.f, 0.f, 0.f})};
    if (!checks.is_valid(shooter, TEXT("Laser shooter is spawned")) ||
        !checks.is_valid(target, TEXT("Laser target is spawned"))) {
        return;
    }

    shooter->set_team(ETestTeam::Blue);
    shooter->set_initial_spawn_delay(60.f);
    shooter->set_spawn_cooldown(60.f);
    target->set_team(ETestTeam::Red);
    target->set_health(scenario_ == ELaserLifecycleScenario::SimultaneousLethalHits
                           ? low_target_health
                           : normal_target_health);
    target->set_initial_spawn_delay(60.f);
    target->set_spawn_cooldown(60.f);

    if (scenario_ == ELaserLifecycleScenario::WorldBlocker) {
        auto* const blocker{spawn_visibility_blocker(
            context_.world, FTransform{FVector::ZeroVector}, TEXT("laser_world_blocker"))};
        checks.is_valid(blocker, TEXT("Laser world blocker is spawned"));
    }

    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                           &FLaserLifecycleScenario::bind_fixture);
}

void FLaserLifecycleScenario::bind_fixture(FProxyEntityMap const& proxy_entities) {
    TArray<FProxyEntityBinding> const bindings{
        {TEXT("laser_shooter"), &shooter_handle, nullptr},
        {TEXT("laser_target"), &target_handle, nullptr},
    };
    resolve_proxy_entity_bindings(proxy_entities, bindings, checks);
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FLaserLifecycleScenario::begin_scenario() {
    driver = TestSimulationDriver::from_world(context_.world);
    auto& orchestrator{driver->orchestrator};
    orchestrator.start_simulation();
    checks.is_true(shooter_handle.is_valid(), TEXT("Laser shooter handle is bound"));
    checks.is_true(target_handle.is_valid(), TEXT("Laser target handle is bound"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const end_time{scenario_ == ELaserLifecycleScenario::Miss ? expiry_test_end_time
                                                                   : collision_test_end_time};
    reset_and_reserve_time_series(orchestrator, end_time, samples);
    orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FLaserLifecycleScenario::on_end_tick));
    driver->timeline.at(projectile_queue_time, [this] { queue_projectiles(); }).finish_at(end_time);
}

void FLaserLifecycleScenario::queue_projectiles() {
    auto* const lasers{const_cast<ATestLasers*>(driver->orchestrator.get_lasers())};
    check(lasers);

    auto const& entity_data{driver->registry.get_entity_data()};
    auto const shooter_location{driver->registry.get_location(shooter_handle)};
    auto const target_location{driver->registry.get_location(target_handle)};
    auto const shooter_radius{entity_data.radii[shooter_handle.index]};
    auto const target_direction{(target_location - shooter_location).GetSafeNormal()};
    auto start{shooter_location + target_direction * (shooter_radius + 100.f)};
    auto fire_direction{target_direction};
    if (scenario_ == ELaserLifecycleScenario::Miss) {
        start = FVector3f{0.f, 0.f, 100000.f};
        fire_direction = FVector3f{0.f, 0.f, 1.f};
    } else if (scenario_ == ELaserLifecycleScenario::WorldBlocker) {
        start = FVector3f{-1000.f, 0.f, 0.f};
        fire_direction = FVector3f{1.f, 0.f, 0.f};
    }
    auto const projectile_count{scenario_ == ELaserLifecycleScenario::SimultaneousLethalHits ? 2
                                                                                             : 1};

    ATestLasers::SpawnRequests requests;
    requests.add_uninitialised(projectile_count);
    for (int32 i{0}; i < projectile_count; ++i) {
        ml::assign(requests.locations, i, start);
        ml::assign(requests.rotations, i, fire_direction.Rotation());
        ml::assign(requests.base_velocities, i, FVector3f::ZeroVector);
        requests.damages[i] = projectile_damage;
        requests.speeds[i] = projectile_speed;
        requests.max_distances[i] =
            scenario_ == ELaserLifecycleScenario::Miss ? miss_max_distance : collision_max_distance;
        requests.instigator_handles[i] = shooter_handle;
        requests.colours[i] = FLinearColor::White;
    }
    lasers->queue_laser_spawns(requests);
}

void FLaserLifecycleScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    auto const* const lasers{orchestrator.get_lasers()};
    check(lasers);
    auto const& registry{driver->registry};
    samples.add(driver->get_time(),
                FSample{
                    .active_lasers = lasers->get_num_instances(),
                    .total_spawned = lasers->get_number_spawned(),
                    .target_health = registry.get_health(target_handle),
                    .alive_entities = registry.count_alive(),
                    .kills = registry.count_kills(),
                });
    driver->timeline.tick(driver->get_time());
}

void FLaserLifecycleScenario::check_scenario() {
    checks.is_true(!samples.is_empty(), TEXT("Laser lifecycle samples are recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto observed_committed_projectile{false};
    auto projectile_damage_was_delayed{false};
    auto const expected_spawn_count{
        scenario_ == ELaserLifecycleScenario::SimultaneousLethalHits ? 2 : 1};
    for (int32 i{0}; i < samples.num(); ++i) {
        auto const& sample{samples.value_at(i)};
        if (sample.active_lasers == expected_spawn_count) {
            observed_committed_projectile = true;
            projectile_damage_was_delayed |=
                sample.target_health ==
                (scenario_ == ELaserLifecycleScenario::SimultaneousLethalHits
                     ? low_target_health
                     : normal_target_health);
        }
    }
    checks.is_true(observed_committed_projectile,
                   TEXT("Queued projectile is committed and observable"));
    checks.is_true(projectile_damage_was_delayed,
                   TEXT("Projectile commit precedes collision damage"));

    auto const& final{samples.last_value()};
    checks.are_equal(
        expected_spawn_count, final.total_spawned, TEXT("Expected projectile count spawned"));
    checks.are_equal(
        0, final.active_lasers, TEXT("Projectile lifecycle ends with no active lasers"));

    switch (scenario_) {
        case ELaserLifecycleScenario::Hit: {
            checks.are_equal(normal_target_health - projectile_damage,
                             final.target_health,
                             TEXT("One projectile applies damage exactly once"));
            checks.are_equal(
                2, final.alive_entities, TEXT("Nonlethal projectile preserves both entities"));
            checks.are_equal(0, final.kills, TEXT("Nonlethal projectile creates no kill"));
            break;
        }
        case ELaserLifecycleScenario::SimultaneousLethalHits: {
            checks.is_true(final.target_health <= 0,
                           TEXT("Simultaneous projectiles cause lethal damage"));
            checks.are_equal(1, final.alive_entities, TEXT("Lethal target is removed once"));
            checks.are_equal(1, final.kills, TEXT("Simultaneous lethal hits record one kill"));
            checks.are_equal(
                TestEntityUniqueEntityData::kills_type{1},
                driver->registry.get_kills(driver->registry.find_unique_id(shooter_handle)),
                TEXT("Shooter receives one credited kill"));
            break;
        }
        case ELaserLifecycleScenario::Miss:
        case ELaserLifecycleScenario::WorldBlocker: {
            checks.are_equal(normal_target_health,
                             final.target_health,
                             TEXT("Non-entity projectile termination preserves target health"));
            checks.are_equal(
                2, final.alive_entities, TEXT("Non-entity termination preserves entities"));
            checks.are_equal(0, final.kills, TEXT("Non-entity termination creates no kill"));
            break;
        }
    }
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FLaserLifecycleScenario::run() {
    TestCommandBuilder.Do([this] { begin_scenario(); })
        .Until([this] { return driver->timeline.is_finished(); }, FTimespan{0, 0, 3})
        .Then([this] { check_scenario(); });
}
}
