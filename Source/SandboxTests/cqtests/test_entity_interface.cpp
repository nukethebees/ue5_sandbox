#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestStaticTurretsProxy.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/test_setup.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(EntityInterfaceTest, "Sandbox.FunctionalTests")
{
    using ThisClass = EntityInterfaceTest;
    using time_type = ml::TestSimulationDriver::time_type;

    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions checks;
    TOptional<ml::TestSimulationDriver> test_driver{NullOpt};

    ATestBatchOrchestrator const* orchestrator{nullptr};
    ATestEntityRegistry const* registry{nullptr};
    ATestCapitalShips const* capitals{nullptr};

    ml::TimeSeriesData<int32> capital_proxy_counts;
    ml::TimeSeriesData<int32> turret_proxy_counts;
    ml::TimeSeriesData<TArray<FRegistryEntityHandle>> capital_target_handles;
    ml::TimeSeriesData<TArray<uint8>> capital_target_alive;

    BEFORE_EACH()
    { spawner = ml::level_test_setup(TEXT("FuncT_proxy_base"), TestRunner, checks); }
    AFTER_EACH()
    { test_driver->orchestrator.clear_end_tick_test_hook(); }
  private:
    static constexpr time_type test_time{0.25};

    void initial_setup() {
        auto& world{spawner->GetWorld()};
        test_driver = ml::TestSimulationDriver::from_world(world);
        test_driver->orchestrator.start_simulation();

        for (TActorIterator<ATestBatchOrchestrator> it(&world); it; ++it) {
            orchestrator = *it;
            break;
        }
        ASSERT_THAT(IsNotNull(orchestrator));

        registry = orchestrator->get_entity_registry();
        ASSERT_THAT(IsNotNull(registry));

        capitals = orchestrator->get_capital_ships();
        ASSERT_THAT(IsNotNull(capitals));
    }

    void sample_values(ATestBatchOrchestrator&) {
        auto const t{test_driver->get_time()};
        auto& world{spawner->GetWorld()};

        capital_proxy_counts.add(t, ml::get_actors<ATestCapitalShipProxy>(world).Num());
        turret_proxy_counts.add(t, ml::get_actors<ATestStaticTurretsProxy>(world).Num());

        TArray<FRegistryEntityHandle> target_handles;
        target_handles.Append(capitals->get_target_handles());
        capital_target_handles.add(t, MoveTemp(target_handles));

        TArray<uint8> target_alive;
        for (auto const handle : capitals->get_target_handles()) {
            target_alive.Add(registry->is_valid_alive(handle));
        }
        capital_target_alive.add(t, MoveTemp(target_alive));
    }

    void main_checks() {
        auto const i{capital_target_handles.nearest_index(test_time)};
        check_no_proxies_alive(i);
        check_capital_targets(i);

        SANDBOX_TESTS_ASSERT_ALL_PASSED(checks);
    }

    void check_no_proxies_alive(int32 const i) {
        checks.are_equal(0, capital_proxy_counts.value_at(i), TEXT("ATestCapitalShipProxy check"));
        checks.are_equal(0, turret_proxy_counts.value_at(i), TEXT("ATestStaticTurretsProxy check"));
    }
    void check_capital_targets(int32 const i) {
        auto const& target_handles{capital_target_handles.value_at(i)};
        auto const& target_alive{capital_target_alive.value_at(i)};
        auto const n_target_handles{target_handles.Num()};

        for (int32 target_index{0}; target_index < n_target_handles; ++target_index) {
            checks.is_true(target_alive[target_index] != 0,
                           FString::Printf(TEXT("Target check: %d"), target_index));
        }
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] {
                initial_setup();
                test_driver->orchestrator.set_end_tick_test_hook(
                    FOrchestratorEndTickTestHook::CreateRaw(this, &ThisClass::sample_values));
                test_driver->set_delta_time_wait(test_time);
            })
            .Until([this] { return test_driver->time_wait_completed(); }, FTimespan{0, 0, 1})
            .Then([this] { main_checks(); });
    }
};
