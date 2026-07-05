#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestCapitalShips.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <EngineUtils.h>

TEST_CLASS(CapitalFighterHandles, "Sandbox.FunctionalTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};

    ATestBatchOrchestrator const* orchestrator{nullptr};
    ATestEntityRegistry const* registry{nullptr};
    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};

    BEFORE_EACH()
    {
        spawner = MakeUnique<FMapTestSpawner>(
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
            TEXT("FuncT_capital_fighter_handles"));
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }
  protected:
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

        fighters = orchestrator->get_capital_ship_fighters();
        ASSERT_THAT(IsNotNull(fighters));
    }
    bool wait_for_fighters_to_spawn() {
        auto const n_fighters{capitals->get_fighters_spawned()};
        if (n_fighters > 0) { return true; }

        return false;
    }
    void run_checks() {
        ASSERT_THAT(IsTrue(capitals->get_fighters_spawned() > 0));
    }

    TEST_METHOD(MainTest)
    {

        FTimespan const timeout{0, 0, 3};
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] { initial_setup(); })
            .Until([this]() -> bool { return wait_for_fighters_to_spawn(); }, timeout)
            .Then([this] { run_checks(); });
    }
};
