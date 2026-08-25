#include "test_simulation_core_regressions_scenario.h"

#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/time_series_test_data.h>

#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShips.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

namespace ml {
namespace {
constexpr double nonlethal_damage_time{0.05};
constexpr double lethal_damage_time{0.15};
constexpr double damage_test_end_time{0.25};
constexpr int32 initial_health{100};
}

FSimulationCoreRegressionScenario::FSimulationCoreRegressionScenario(
    FSimulationTestContext& context, ESimulationCoreRegressionScenario const scenario)
    : FSimulationTestScenario{context}
    , scenario_{scenario} {
    if (scenario_ == ESimulationCoreRegressionScenario::DamageLifecycle) {
        TestCommandBuilder.Do([this] { spawn_damage_fixture(); });
    }
}

void FSimulationCoreRegressionScenario::tear_down() {
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
    auto const& registry{test_driver->registry};
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
    checks.is_true(test_driver->registry.is_valid_dead(damaged_handle),
                   TEXT("Killed handle remains valid-dead"));
    checks.are_equal(
        0, test_driver->registry.count_kills(), TEXT("Unattributed death does not create a kill"));
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
}
