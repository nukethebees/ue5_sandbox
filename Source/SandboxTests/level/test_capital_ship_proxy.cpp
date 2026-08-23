#include <SandboxTests/support/test_setup.h>
#include <SandboxTests/support/TestActorSpawning.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestCapitalShipsConfig.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <SandboxTests/support/SoftTestAssertions.h>

#include <CQTest.h>

namespace {
ml::FTestBatchOrchestratorLevelSetup level_setup{};
}

TEST_CLASS(TestCapitalShipProxy, "Sandbox.LevelTests")
{
    using ThisClass = TestCapitalShipProxy;

    inline static FName const default_health_capital_name{TEXT("default_health_capital")};
    inline static FName const overridden_health_capital_name{TEXT("overridden_health_capital")};

    ml::FSoftTestAssertions checks{};
    FRegistryEntityHandle default_health_handle;
    FRegistryEntityHandle overridden_health_handle;
    TestEntityUniqueId default_health_unique_id;
    TestEntityUniqueId overridden_health_unique_id;
    int32 default_health{0};
    int32 overridden_health{0};
    bool proxy_handles_bound{false};

    BEFORE_EACH()
    {
        checks.test_runner = TestRunner;
        checks.all_passed = true;
        default_health_handle = {};
        overridden_health_handle = {};
        default_health_unique_id = {};
        overridden_health_unique_id = {};
        default_health = 0;
        overridden_health = 0;
        proxy_handles_bound = false;

        level_setup.begin_test(TestCommandBuilder, *TestRunner, checks);
        TestCommandBuilder.Do(
            [this] { spawn_proxies(level_setup.get_world(), level_setup.get_config()); });
    }

    AFTER_EACH()
    {
        ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
        level_setup.end_test();
    }

    AFTER_ALL()
    {
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

        ml::spawn_capital_proxy(
            world, config, checks, default_health_capital_name, FVector::ZeroVector);

        auto* const overridden_health_proxy{ml::spawn_capital_proxy(
            world, config, checks, overridden_health_capital_name, FVector{2000.f, 0.f, 0.f})};
        if (!IsValid(overridden_health_proxy)) {
            return;
        }
        overridden_health_proxy->set_health(overridden_health);

        ATestBatchOrchestrator::on_proxy_entities_bound.AddRaw(this,
                                                               &ThisClass::resolve_proxy_handles);
    }

    void resolve_proxy_handles(FProxyEntityMap const& proxy_entities) {
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

        checks.is_true(default_health_handle.is_valid(),
                       TEXT("Nullopt-health capital proxy is bound"));
        checks.is_true(overridden_health_handle.is_valid(),
                       TEXT("Overridden-health capital proxy is bound"));
        checks.is_true(default_health_unique_id.is_valid(),
                       TEXT("Nullopt-health capital proxy has a unique ID"));
        checks.is_true(overridden_health_unique_id.is_valid(),
                       TEXT("Overridden-health capital proxy has a unique ID"));

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

        proxy_handles_bound = true;
    }

    void check_proxy_healths() {
        auto test_driver{ml::TestSimulationDriver::from_world(level_setup.get_world())};
        test_driver.orchestrator.start_simulation();

        checks.is_true(proxy_handles_bound,
                       TEXT("Capital proxy handles are resolved when proxies are bound"));
        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);

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
