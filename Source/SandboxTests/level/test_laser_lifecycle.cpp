#include "test_laser_lifecycle_scenario.h"

#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestCollisionActor.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <SpaceGame/combat/lasers/TestLasersSimulation.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

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
        data.static_bounds.mins.set(0, FVector3f{-1100.f, -100.f, -100.f});
        data.static_bounds.maxes.set(0, FVector3f{-900.f, 100.f, 100.f});
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto* const lasers{harness.get_simulation().get_lasers()};
    auto const* capitals{harness.get_simulation().get_capital_ships()};
    auto const shooter{capitals->get_handle(0)};
    auto const target{capitals->get_handle(1)};
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
                    Sample{lasers->get_num_instances(),
                           lasers->get_number_spawned(),
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
            requests.colours[i] = FLinearColor::White;
        }
        lasers->queue_laser_spawns(requests);
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

    if (scenario_ == ELaserLifecycleScenario::WorldBlocker) {
        auto& config{const_cast<USpaceGameLevelConfig&>(context_.config)};
        config.collision_grid.harvested_collision_actor_classes = previous_harvested_classes;
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
        auto& config{const_cast<USpaceGameLevelConfig&>(context_.config)};
        previous_harvested_classes = config.collision_grid.harvested_collision_actor_classes;
        config.collision_grid.harvested_collision_actor_classes.AddUnique(
            ASandboxTestCollisionActor::StaticClass());

        world_blocker = context_.world.SpawnActor<ASandboxTestDerivedCollisionActor>(
            ASandboxTestDerivedCollisionActor::StaticClass(),
            FTransform{FVector{-1000.f, 0.f, 0.f}});
        checks.is_valid(world_blocker.Get(), TEXT("Laser world blocker is spawned"));
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
    if (scenario_ == ELaserLifecycleScenario::WorldBlocker && world_blocker.IsValid()) {
        checks.is_true(world_blocker->get_collision_component()->GetCollisionEnabled() ==
                           ECollisionEnabled::NoCollision,
                       TEXT("Harvested world blocker has Unreal collision disabled"));
    }
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const end_time{scenario_ == ELaserLifecycleScenario::Miss ? expiry_test_end_time
                                                                   : collision_test_end_time};
    reset_and_reserve_time_series(orchestrator, end_time, samples);
    orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FLaserLifecycleScenario::on_end_tick));
    driver->timeline.at(projectile_queue_time, [this] { queue_projectiles(); }).finish_at(end_time);
}

void FLaserLifecycleScenario::queue_projectiles() {
    auto* const lasers{driver->orchestrator.get_lasers()};
    check(lasers);

    auto const& entity_data{driver->get_registry().get_entity_data()};
    auto const shooter_location{driver->get_registry().get_location(shooter_handle)};
    auto const target_location{driver->get_registry().get_location(target_handle)};
    auto const shooter_radius{entity_data.radii[shooter_handle.index]};
    auto const target_direction{(target_location - shooter_location).GetSafeNormal()};
    auto start{shooter_location + target_direction * (shooter_radius + 100.f)};
    auto fire_direction{target_direction};
    if (scenario_ == ELaserLifecycleScenario::Miss) {
        start = FVector3f{0.f, 0.f, 100000.f};
        fire_direction = FVector3f{0.f, 0.f, 1.f};
    } else if (scenario_ == ELaserLifecycleScenario::WorldBlocker) {
        start = FVector3f{-2000.f, 0.f, 0.f};
        fire_direction = FVector3f{1.f, 0.f, 0.f};
    }
    auto const projectile_count{scenario_ == ELaserLifecycleScenario::SimultaneousLethalHits ? 2
                                                                                             : 1};

    ml::test_lasers::SpawnRequests requests;
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
    auto const& registry{driver->get_registry()};
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
            checks.are_equal(TestEntityUniqueEntityData::kills_type{1},
                             driver->get_registry().get_kills(
                                 driver->get_registry().find_unique_id(shooter_handle)),
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
