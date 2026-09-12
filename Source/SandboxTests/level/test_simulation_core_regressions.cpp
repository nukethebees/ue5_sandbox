#include "test_simulation_core_regressions_scenario.h"

#include <SandboxTests/support/SimulationTestAssets.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>
#include <SandboxTests/support/WorldlessSimulationTest.h>

#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipSimulation.h>

namespace ml {
namespace {
constexpr double nonlethal_damage_time{0.05};
constexpr double lethal_damage_time{0.15};
constexpr double damage_test_end_time{0.25};
constexpr int32 initial_health{100};
}

namespace collision_damage_test {
inline constexpr int32 player_health{1000};
inline constexpr int32 capital_health{150};
inline constexpr int32 overlap_damage{50};
inline constexpr double duration{0.1};
}

void run_worldless_simulation_core_regression(FAutomationTestBase& test,
                                              FSoftTestAssertions& checks,
                                              USpaceGameLevelConfig const& config,
                                              ESimulationCoreRegressionScenario const scenario) {
    auto data{make_worldless_simulation_test_data(config)};
    if (scenario == ESimulationCoreRegressionScenario::DamageLifecycle) {
        data.capital_ships.fighter_spawn_slots = 0;
        data.capital_ships.fighter_spawn_slots_relative_transforms.Reset();
        data.capital_spawns.add_defaulted(1);
        data.capital_spawns.teams[0] = ETestTeam::White;
        data.capital_spawns.healths[0] = initial_health;
        data.capital_spawns.initial_spawn_delays[0] = 60.f;
        data.capital_spawns.spawn_cooldowns[0] = 60.f;
    }

    FWorldlessSimulationTest harness{MoveTemp(data)};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};
    if (scenario == ESimulationCoreRegressionScenario::FixedTickLifecycle) {
        int32 end_tick_calls{};
        harness.on_end_tick = [&](FLevelSimulation&) { ++end_tick_calls; };
        simulation.start();
        auto const period{simulation.get_clock().get_tick_period()};
        simulation.advance(period * 0.5);
        checks.are_equal(uint64{0},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Half tick is accumulated"));
        simulation.advance(period * 0.5);
        checks.are_equal(uint64{1},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Two half ticks advance once"));
        checks.are_equal(1, end_tick_calls, TEXT("End-tick hook executes once per completed tick"));
        simulation.advance(period * 3.25);
        checks.are_equal(uint64{4},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Large delta catches up deterministically"));
        checks.are_equal(4, end_tick_calls, TEXT("Catch-up executes every end-tick hook"));
        checks.are_equal(period * 4.0,
                         simulation.get_clock().get_simulation_time(),
                         1.e-9,
                         TEXT("Simulation time derives from completed ticks"));
        simulation.pause();
        simulation.advance(period * 10.0);
        checks.are_equal(uint64{4},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Paused simulation ignores time"));
        simulation.start();
        simulation.advance(period * 0.75);
        checks.are_equal(uint64{5},
                         simulation.get_clock().get_completed_ticks(),
                         TEXT("Resume preserves accumulated fraction"));
        checks.are_equal(5, end_tick_calls, TEXT("Resumed tick executes one hook"));
        return;
    }

    auto const damaged_handle{simulation.get_capital_ships().get_handle(0)};
    struct DamageSample {
        int32 capital_count{};
        int32 registry_alive_count{};
        int32 health{};
        int32 telemetry_active_count{};
    };
    TimeSeriesData<DamageSample> samples;
    harness.on_end_tick = [&](FLevelSimulation& level) {
        auto const& registry{harness.get_registry()};
        auto const& telemetry{level.get_level_telemetry_manager().get_active_entity_count_data()};
        samples.add(harness.get_time(),
                    DamageSample{
                        .capital_count = level.get_capital_ships().get_num_instances(),
                        .registry_alive_count = registry.count_alive(),
                        .health = registry.is_valid_handle(damaged_handle)
                                    ? registry.get_health(damaged_handle)
                                    : 0,
                        .telemetry_active_count = telemetry.last_value(),
                    });
    };
    harness.timeline.at(nonlethal_damage_time,
                        [&] { harness.queue_damage(TArray{damaged_handle}, 25); });
    harness.timeline.at(lethal_damage_time,
                        [&] { harness.queue_damage(TArray{damaged_handle}, 75); });
    harness.timeline.finish_at(damage_test_end_time);
    test.TestTrue(TEXT("Damage lifecycle timeline completes"),
                  harness.run_until_timeline_finished(1.0));
    checks.is_true(!samples.is_empty(), TEXT("Damage lifecycle samples are recorded"));
    if (samples.is_empty()) {
        return;
    }
    auto const& initial{samples.value_at(0)};
    auto const& nonlethal{samples.nearest_value(0.10)};
    auto const& lethal{samples.nearest_value(0.22)};
    checks.are_equal(1, initial.capital_count, TEXT("One capital starts active"));
    checks.are_equal(initial_health, initial.health, TEXT("Capital starts at configured health"));
    checks.are_equal(1, nonlethal.capital_count, TEXT("Nonlethal damage preserves batch entity"));
    checks.are_equal(75, nonlethal.health, TEXT("Nonlethal damage is applied once"));
    checks.are_equal(
        0, lethal.capital_count, TEXT("Lethal damage removes batch entity in resolution tick"));
    checks.are_equal(
        0, lethal.registry_alive_count, TEXT("Registry death commits in the same tick"));
    checks.are_equal(
        0, lethal.telemetry_active_count, TEXT("Telemetry observes committed death before hook"));
    checks.are_equal(
        0, lethal.health, TEXT("Registry retains terminal health for the dead handle"));
    checks.is_true(harness.get_registry().is_valid_dead(damaged_handle),
                   TEXT("Killed handle remains valid-dead"));
    checks.are_equal(
        0, harness.get_registry().count_kills(), TEXT("Unattributed death does not create a kill"));
}

FSimulationCoreRegressionScenario::FSimulationCoreRegressionScenario(
    FSimulationTestContext& context, ESimulationCoreRegressionScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {
    if (scenario_ == ESimulationCoreRegressionScenario::DamageLifecycle) {
        TestCommandBuilder.Do([this] { spawn_damage_fixture(); });
    }
}

void FSimulationCoreRegressionScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    if (test_driver.IsSet()) {
        test_driver->orchestrator.clear_end_tick_test_hook();
        test_driver->orchestrator.pause_simulation();
    }
}

void FSimulationCoreRegressionScenario::spawn_damage_fixture() {
    auto* const proxy{spawn_capital_proxy(
        context_.world, context_.config, checks, TEXT("damage_target"), FVector::ZeroVector)};
    if (!checks.is_valid(proxy, TEXT("Damage lifecycle capital is spawned"))) {
        return;
    }

    proxy->set_health(initial_health);
    proxy->set_initial_spawn_delay(60.f);
    proxy->set_spawn_cooldown(60.f);
    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
        this, &FSimulationCoreRegressionScenario::bind_damage_fixture);
}

void FSimulationCoreRegressionScenario::bind_damage_fixture(FProxyEntityMap const& proxy_entities) {
    TArray<FProxyEntityBinding> const bindings{
        {TEXT("damage_target"), &damaged_handle, nullptr},
    };
    resolve_proxy_entity_bindings(proxy_entities, bindings, checks);
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FSimulationCoreRegressionScenario::run_fixed_tick_lifecycle() {
    test_driver = TestSimulationDriver::from_world(context_.world);
    auto& orchestrator{test_driver->orchestrator};
    test_driver->set_time_scale(1.0);
    orchestrator.start_simulation();
    orchestrator.SetActorTickEnabled(false);
    orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateLambda(
        [this](ATestBatchOrchestrator&) { ++end_tick_calls; }));

    auto const period{orchestrator.get_tick_period()};
    orchestrator.tick(period * 0.5);
    checks.are_equal(
        uint64{0}, orchestrator.get_completed_ticks(), TEXT("Half tick is accumulated"));

    orchestrator.tick(period * 0.5);
    checks.are_equal(
        uint64{1}, orchestrator.get_completed_ticks(), TEXT("Two half ticks advance once"));
    checks.are_equal(1, end_tick_calls, TEXT("End-tick hook executes once per completed tick"));

    orchestrator.tick(period * 3.25);
    checks.are_equal(uint64{4},
                     orchestrator.get_completed_ticks(),
                     TEXT("Large delta catches up deterministically"));
    checks.are_equal(4, end_tick_calls, TEXT("Catch-up executes every end-tick hook"));
    checks.are_equal(period * 4.0,
                     orchestrator.get_simulation_time(),
                     1.e-9,
                     TEXT("Simulation time derives from completed ticks"));

    orchestrator.pause_simulation();
    orchestrator.tick(period * 10.0);
    checks.are_equal(
        uint64{4}, orchestrator.get_completed_ticks(), TEXT("Paused simulation ignores time"));

    orchestrator.start_simulation();
    orchestrator.SetActorTickEnabled(false);
    orchestrator.tick(period * 0.75);
    checks.are_equal(uint64{5},
                     orchestrator.get_completed_ticks(),
                     TEXT("Resume preserves accumulated fraction"));
    checks.are_equal(5, end_tick_calls, TEXT("Resumed tick executes one hook"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FSimulationCoreRegressionScenario::begin_damage_lifecycle() {
    test_driver = TestSimulationDriver::from_world(context_.world);
    auto& orchestrator{test_driver->orchestrator};
    orchestrator.start_simulation();
    checks.is_true(damaged_handle.is_valid(), TEXT("Damage target handle is bound"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    reset_and_reserve_time_series(orchestrator, damage_test_end_time, damage_samples);
    orchestrator.set_end_tick_test_hook(FOrchestratorEndTickTestHook::CreateRaw(
        this, &FSimulationCoreRegressionScenario::on_damage_end_tick));
    test_driver->timeline.at(nonlethal_damage_time,
                             [this] { test_driver->queue_damage(TArray{damaged_handle}, 25); });
    test_driver->timeline.at(lethal_damage_time,
                             [this] { test_driver->queue_damage(TArray{damaged_handle}, 75); });
    test_driver->timeline.finish_at(damage_test_end_time);
}

void FSimulationCoreRegressionScenario::on_damage_end_tick(ATestBatchOrchestrator& orchestrator) {
    auto const* const capitals{orchestrator.get_capital_ships()};
    check(capitals);
    auto const& registry{test_driver->get_registry()};
    auto const& telemetry{
        orchestrator.get_level_telemetry_manager().get_active_entity_count_data()};
    damage_samples.add(test_driver->get_time(),
                       FDamageSample{
                           .capital_count = capitals->get_num_instances(),
                           .registry_alive_count = registry.count_alive(),
                           .health = registry.is_valid_handle(damaged_handle)
                                       ? registry.get_health(damaged_handle)
                                       : 0,
                           .telemetry_active_count = telemetry.last_value(),
                       });
    test_driver->timeline.tick(test_driver->get_time());
}

void FSimulationCoreRegressionScenario::check_damage_lifecycle() {
    checks.is_true(!damage_samples.is_empty(), TEXT("Damage lifecycle samples are recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& initial{damage_samples.value_at(0)};
    auto const& nonlethal{damage_samples.nearest_value(0.10)};
    auto const& lethal{damage_samples.nearest_value(0.22)};
    checks.are_equal(1, initial.capital_count, TEXT("One capital starts active"));
    checks.are_equal(initial_health, initial.health, TEXT("Capital starts at configured health"));
    checks.are_equal(1, nonlethal.capital_count, TEXT("Nonlethal damage preserves batch entity"));
    checks.are_equal(75, nonlethal.health, TEXT("Nonlethal damage is applied once"));
    checks.are_equal(
        0, lethal.capital_count, TEXT("Lethal damage removes batch entity in resolution tick"));
    checks.are_equal(
        0, lethal.registry_alive_count, TEXT("Registry death commits in the same tick"));
    checks.are_equal(
        0, lethal.telemetry_active_count, TEXT("Telemetry observes committed death before hook"));
    checks.are_equal(
        0, lethal.health, TEXT("Registry retains terminal health for the dead handle"));
    checks.is_true(test_driver->get_registry().is_valid_dead(damaged_handle),
                   TEXT("Killed handle remains valid-dead"));
    checks.are_equal(0,
                     test_driver->get_registry().count_kills(),
                     TEXT("Unattributed death does not create a kill"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FSimulationCoreRegressionScenario::run() {
    if (scenario_ == ESimulationCoreRegressionScenario::FixedTickLifecycle) {
        TestCommandBuilder.Do([this] { run_fixed_tick_lifecycle(); });
        return;
    }

    TestCommandBuilder.Do([this] { begin_damage_lifecycle(); })
        .Until([this] { return test_driver->timeline.is_finished(); }, FTimespan{0, 0, 2})
        .Then([this] { check_damage_lifecycle(); });
}

FCollisionDamageScenario::FCollisionDamageScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_fixture(); });
}

void FCollisionDamageScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    if (test_driver.IsSet()) {
        test_driver->orchestrator.clear_end_tick_test_hook();
        test_driver->orchestrator.pause_simulation();
    }
}

void FCollisionDamageScenario::spawn_fixture() {
    auto* const level_config{duplicate_level_config(context_.config, context_.orchestrator)};
    if (!checks.not_nullptr(level_config, TEXT("Collision test level config is duplicated"))) {
        return;
    }

    level_config->player_ship.lateral_adjustment_speed = 1.f;
    context_.orchestrator.set_level_config(*level_config);

    auto* const player{spawn_player_ship(context_.world,
                                         level_config->classes.player_ship_class,
                                         &level_config->player_ship,
                                         FTransform::Identity)};
    if (!checks.is_valid(player, TEXT("Collision test player is spawned"))) {
        return;
    }

    player->set_flight_mode(ETestSpaceShipFlightMode::PlanarVelocity);
    context_.orchestrator.set_player_ship(*player);

    auto* const capital{spawn_capital_proxy(
        context_.world, *level_config, checks, TEXT("collision_target"), FVector::ZeroVector)};
    if (!checks.is_valid(capital, TEXT("Collision test capital is spawned"))) {
        return;
    }

    capital->set_health(collision_damage_test::capital_health);
    capital->set_initial_spawn_delay(60.f);
    capital->set_spawn_cooldown(60.f);
    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                           &FCollisionDamageScenario::bind_fixture);
}

void FCollisionDamageScenario::bind_fixture(FProxyEntityMap const& proxy_entities) {
    TArray<FProxyEntityBinding> const bindings{
        {TEXT("collision_target"), &capital_handle, &capital_id},
    };
    resolve_proxy_entity_bindings(proxy_entities, bindings, checks);
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FCollisionDamageScenario::begin_test() {
    auto& driver{initialise_test_driver()};
    driver.set_time_scale(1.0);
    driver.orchestrator.start_simulation();

    auto* const player{driver.orchestrator.get_player_ship_simulation()};
    checks.not_nullptr(player, TEXT("Collision test player simulation is available"));
    checks.is_true(capital_handle.is_valid(), TEXT("Collision test capital handle is bound"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    player_handle = player->registry_handle;
    player->add_health(collision_damage_test::player_health - player->health.health);
    player->set_lateral_move_input(1.f);

    reset_and_reserve_time_series(driver.orchestrator, collision_damage_test::duration, samples);
    driver.orchestrator.set_end_tick_test_hook(
        FOrchestratorEndTickTestHook::CreateRaw(this, &FCollisionDamageScenario::on_end_tick));
    driver.timeline.finish_after(collision_damage_test::duration);
}

void FCollisionDamageScenario::on_end_tick(ATestBatchOrchestrator& orchestrator) {
    auto const& registry{test_driver->get_registry()};
    auto const events{
        orchestrator.get_spatial_query_manager().get_collision_system().get_aabb_overlap_events()};
    int32 dynamic_overlap_count{};
    if (!events.batches.IsEmpty()) {
        dynamic_overlap_count =
            events.get_batch(events.batches.Num() - 1).overlaps.entity_entity_overlaps.num();
    }

    samples.add(test_driver->get_time(),
                FSample{
                    .player_health = registry.get_health(player_handle),
                    .capital_health = registry.get_health(capital_handle),
                    .dynamic_overlap_count = dynamic_overlap_count,
                    .kill_count = registry.count_kills(),
                    .player_alive = registry.is_valid_alive(player_handle),
                });
    test_driver->advance_timeline();
}

void FCollisionDamageScenario::check_results() {
    checks.is_greater_than(samples.num(), 2, TEXT("Three collision ticks are recorded"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& first{samples.value_at(0)};
    auto const& second{samples.value_at(1)};
    auto const& third{samples.value_at(2)};
    checks.are_equal(1, first.dynamic_overlap_count, TEXT("First tick detects one overlap"));
    checks.are_equal(1, second.dynamic_overlap_count, TEXT("Second tick detects one overlap"));
    checks.are_equal(1, third.dynamic_overlap_count, TEXT("Third tick detects one overlap"));
    checks.are_equal(collision_damage_test::player_health - collision_damage_test::overlap_damage,
                     first.player_health,
                     TEXT("First overlap damages the player"));
    checks.are_equal(collision_damage_test::player_health -
                         2 * collision_damage_test::overlap_damage,
                     second.player_health,
                     TEXT("Second overlap damages the player"));
    checks.are_equal(collision_damage_test::player_health -
                         3 * collision_damage_test::overlap_damage,
                     third.player_health,
                     TEXT("Third overlap damages the player"));
    checks.are_equal(collision_damage_test::capital_health - collision_damage_test::overlap_damage,
                     first.capital_health,
                     TEXT("First overlap damages the capital"));
    checks.are_equal(collision_damage_test::capital_health -
                         2 * collision_damage_test::overlap_damage,
                     second.capital_health,
                     TEXT("Second overlap damages the capital"));
    checks.are_equal(collision_damage_test::capital_health -
                         3 * collision_damage_test::overlap_damage,
                     third.capital_health,
                     TEXT("Third overlap kills the capital"));
    checks.is_true(third.player_alive, TEXT("Player survives the third overlap tick"));
    checks.is_true(test_driver->get_registry().is_valid_dead(capital_handle),
                   TEXT("Capital death commits in the third overlap tick"));
    checks.are_equal(0, third.kill_count, TEXT("Collision death grants no combat kill"));
    checks.is_true(test_driver->get_registry().get_unique_entities().death_reason[capital_id.id] ==
                       ETestDeathReason::Unknown,
                   TEXT("Collision death is environmental"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}

void FCollisionDamageScenario::run() {
    run_until_timeline_finished(
        [this] { begin_test(); }, FTimespan{0, 0, 2}, [this] { check_results(); });
}
}
