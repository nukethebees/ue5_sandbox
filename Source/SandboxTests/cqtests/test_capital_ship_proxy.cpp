#include "test_setup.h"
#include "TestActorSpawning.h"
#include "TestSimulationDriver.h"

#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>

#include <CQTest.h>

TEST_CLASS(TestCapitalShipProxy, "Sandbox.FunctionalTests")
{
    using ThisClass = TestCapitalShipProxy;

    ml::FTestBatchOrchestratorLevelSetup level_setup;
    ml::FSoftTestAssertions checks{};
    ATestCapitalShipProxy* default_health_proxy{nullptr};
    ATestCapitalShipProxy* overridden_health_proxy{nullptr};
    FRegistryEntityHandle default_health_handle;
    FRegistryEntityHandle overridden_health_handle;
    int32 default_health{0};
    int32 overridden_health{0};
    bool proxy_handles_bound{false};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        default_health_proxy = nullptr;
        overridden_health_proxy = nullptr;
        default_health_handle = {};
        overridden_health_handle = {};
        default_health = 0;
        overridden_health = 0;
        proxy_handles_bound = false;

        level_setup.setup(TestCommandBuilder,
                          *TestRunner,
                          [this](UWorld& world, UTestSimulationConfig const& config) {
                              spawn_proxies(world, config);
                          });
    }

    AFTER_EACH()
    {
        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
        level_setup.teardown();
    }

    TEST_METHOD(ProxyHealthOverridesConfig)
    {
        TestCommandBuilder.Do([this] { check_proxy_healths(); });
    }
  private:
    /* ------------------------------------------------------------------------------------------ */
    // Proxy health
    /* ------------------------------------------------------------------------------------------ */
    void spawn_proxies(UWorld & world, UTestSimulationConfig const& config) {
        auto* const capital_config{config.simulation_config->capital_ships_config.Get()};
        check(capital_config);

        default_health = capital_config->max_health;
        overridden_health = default_health + 1234;

        default_health_proxy = &ml::spawn_capital_proxy(world, config, FVector::ZeroVector);
        default_health_proxy->set_health(NullOpt);
        overridden_health_proxy =
            &ml::spawn_capital_proxy(world, config, FVector{2000.f, 0.f, 0.f});
        overridden_health_proxy->set_health(overridden_health);

        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                               &ThisClass::resolve_proxy_handles);
    }

    void resolve_proxy_handles(FProxyEntityMap const& proxy_entities) {
        auto const default_proxy_available{
            checks.is_valid(default_health_proxy, TEXT("Default-health proxy is available"))};
        auto const overridden_proxy_available{
            checks.is_valid(overridden_health_proxy, TEXT("Overridden-health proxy is available"))};

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto const* const default_identifiers{proxy_entities.Find(default_health_proxy)};
        auto const* const overridden_identifiers{proxy_entities.Find(overridden_health_proxy)};
        auto const default_bound{
            checks.not_nullptr(default_identifiers, TEXT("Default-health proxy is bound"))};
        auto const overridden_bound{
            checks.not_nullptr(overridden_identifiers, TEXT("Overridden-health proxy is bound"))};

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        default_health_handle = default_identifiers->handle;
        overridden_health_handle = overridden_identifiers->handle;
        proxy_handles_bound = true;
    }

    void check_proxy_healths() {
        checks.is_true(proxy_handles_bound,
                       TEXT("Capital proxy handles are resolved when proxies are bound"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        auto test_driver{ml::TestSimulationDriver::from_world(level_setup.get_world())};
        auto const& capitals{test_driver.get_capital_ships()};

        checks.are_equal(2,
                         capitals.get_num_instances(),
                         TEXT("Two capital ships are spawned from the proxies"));
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
};
