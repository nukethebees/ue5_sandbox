#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include "test_capital_ship_proxy_scenario.h"

#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <SandboxTests/support/SoftTestAssertions.h>

namespace ml {
FTestCapitalShipProxyScenario::FTestCapitalShipProxyScenario(FSimulationTestContext& context)
    : FSimulationTestScenario{context} {
    TestCommandBuilder.Do([this] { spawn_proxies(context_.world, context_.config); });
}

void FTestCapitalShipProxyScenario::on_tear_down() {
    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}

void FTestCapitalShipProxyScenario::run() {
    TestCommandBuilder.Do([this] { check_proxy_healths(); });
}

/* ------------------------------------------------------------------------------------------ */
// Proxy health
/* ------------------------------------------------------------------------------------------ */
void FTestCapitalShipProxyScenario::spawn_proxies(UWorld& world,
                                                  UTestSimulationConfig const& config) {
    auto* const capital_config{config.simulation_config->capital_ships_config.Get()};
    check(capital_config);

    default_health = capital_config->max_health;
    overridden_health = default_health + 1234;

    ml::spawn_capital_proxy(
        world, config, checks, default_health_capital_name, FVector::ZeroVector);

    auto* const overridden_health_proxy{ml::spawn_capital_proxy(
        world, config, checks, overridden_health_capital_name, FVector{2000.f, 0.f, 0.f})};
    if (!IsValid(overridden_health_proxy)) {
        return;
    }
    overridden_health_proxy->set_health(overridden_health);

    ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(
        this, &FTestCapitalShipProxyScenario::resolve_proxy_handles);
}

void FTestCapitalShipProxyScenario::resolve_proxy_handles(FProxyEntityMap const& proxy_entities) {
    checks.are_equal(proxy_entities.Num(), 2, TEXT("Correct number of proxies"));

    TArray<ml::FProxyEntityBinding> const bindings{
        {.test_name = default_health_capital_name,
         .handle = &default_health_handle,
         .unique_id = &default_health_unique_id},
        {.test_name = overridden_health_capital_name,
         .handle = &overridden_health_handle,
         .unique_id = &overridden_health_unique_id},
    };
    ml::resolve_proxy_entity_bindings(proxy_entities, bindings, checks);

    checks.is_true(default_health_handle.is_valid(), TEXT("Nullopt-health capital proxy is bound"));
    checks.is_true(overridden_health_handle.is_valid(),
                   TEXT("Overridden-health capital proxy is bound"));
    checks.is_true(default_health_unique_id.is_valid(),
                   TEXT("Nullopt-health capital proxy has a unique ID"));
    checks.is_true(overridden_health_unique_id.is_valid(),
                   TEXT("Overridden-health capital proxy has a unique ID"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    proxy_handles_bound = true;
}

void FTestCapitalShipProxyScenario::check_proxy_healths() {
    auto& driver{initialise_test_driver()};
    driver.orchestrator.start_simulation();

    checks.is_true(proxy_handles_bound,
                   TEXT("Capital proxy handles are resolved when proxies are bound"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    auto const& capitals{driver.get_capital_ships()};

    checks.are_equal(
        2, capitals.get_num_instances(), TEXT("Two capital ships are spawned from the proxies"));
    checks.is_true(capitals.is_valid(default_health_handle),
                   TEXT("Default-health proxy has a capital-ship handle"));
    checks.is_true(capitals.is_valid(overridden_health_handle),
                   TEXT("Overridden-health proxy has a capital-ship handle"));
    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

    checks.are_equal(default_health,
                     capitals.get_health(default_health_handle),
                     TEXT("Nullopt proxy health uses the capital-ship config"));
    checks.are_equal(overridden_health,
                     capitals.get_health(overridden_health_handle),
                     TEXT("Proxy health overrides the capital-ship config"));

    SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
}
}
