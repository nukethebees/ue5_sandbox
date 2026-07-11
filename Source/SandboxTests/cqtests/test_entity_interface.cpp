#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestCapitalShips.h>
#include <Sandbox/batch_game/TestStaticTurretsProxy.h>
#include <Sandbox/core/SandboxDeveloperSettings.h>

#include <SandboxTests/cqtests/SoftTestAssertions.h>
#include <SandboxTests/cqtests/TestSimulationDriver.h>

#include <SandboxCore/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <EngineUtils.h>
#include <Misc/Optional.h>

TEST_CLASS(EntityInterfaceTest, "Sandbox.FunctionalTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ml::FSoftTestAssertions<std::remove_cvref_t<decltype(*TestRunner)>> checks;

    ATestBatchOrchestrator const* orchestrator{nullptr};
    ATestEntityRegistry const* registry{nullptr};
    ATestCapitalShips const* capitals{nullptr};

    uint64 tick_wait_end{5};

    bool log_successful_assertions{false};
    bool all_passed{true};

    BEFORE_EACH()
    {
        spawner = MakeUnique<FMapTestSpawner>(
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
            TEXT("FuncT_proxy_base"));
        spawner->AddWaitUntilLoadedCommand(TestRunner);

        checks.test_runner = TestRunner;
        checks.all_passed = true;
    }
  private:
    void initial_setup() {
        auto& world{spawner->GetWorld()};

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

    void main_checks() {
        check_no_proxies_alive();
        check_capital_targets();

        ASSERT_THAT(IsTrue(checks.all_passed, TEXT("all_passed")));
    }

    void check_no_proxies_alive() {
        auto& world{spawner->GetWorld()};
        checks.are_equal(0,
                         ml::get_actors<ATestCapitalShipProxy>(world).Num(),
                         TEXT("ATestCapitalShipProxy check"));
        checks.are_equal(0,
                         ml::get_actors<ATestStaticTurretsProxy>(world).Num(),
                         TEXT("ATestStaticTurretsProxy check"));
    }
    void check_capital_targets() {
        auto const target_handles{capitals->get_target_handles()};
        auto const n_target_handles{target_handles.Num()};

        for (int32 i{0}; i < n_target_handles; ++i) {
            auto const handle{target_handles[i]};
            checks.is_true(registry->is_valid_alive(handle),
                           FString::Printf(TEXT("Target check: %d"), i));
        }
    }

    TEST_METHOD(MainTest)
    {
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this] -> bool { return orchestrator->get_tick_count() == tick_wait_end; },
                   FTimespan{0, 0, 1})
            .Then([this] { main_checks(); });
    }
};
