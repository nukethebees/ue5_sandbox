#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestStaticTurrets.h>

#include <Components/MapTestSpawner.h>
#include <CQTest.h>
#include <EngineUtils.h>

TEST_CLASS(BatchGameFunctionalTests, "Sandbox.FunctionalTests")
{
    // static inline TUniquePtr<FMapTestSpawner> spawner{nullptr};
    TUniquePtr<FMapTestSpawner> spawner{nullptr};
    ATestBatchOrchestrator* orchestrator{nullptr};
    ATestEntityRegistry* registry{nullptr};
    ATestStaticTurrets* turrets{nullptr};

    BEFORE_EACH()
    {
        spawner = MakeUnique<FMapTestSpawner>(
            TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/test_levels/functional_tests"),
            TEXT("FuncT_simple_batch"));
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }
  protected:
    bool test_body() {
        auto const n_ids{registry->get_num_unique_ids_issued()};

        if (n_ids < 1) {
            Assert.Fail(TEXT("n_ids < 1"));
            return true;
        }

        auto const kills{registry->count_kills()};
        auto const n_alive{registry->count_alive()};
        auto const expected_kills{1};
        auto const kill_count_reached{(kills == expected_kills)};

        auto const too_many_kills{kills > expected_kills};
        if (too_many_kills) {
            Assert.Fail(TEXT("too_many_kills"));
            return true;
        }

        auto const n_unique{registry->get_num_unique_ids_issued()};
        auto const n_alive_expected{n_unique - kills};
        if (n_alive != n_alive_expected) {
            Assert.Fail(TEXT("n_alive != n_alive_expected"));
            return true;
        }

        return kill_count_reached;
    }

    TEST_METHOD(FuncT_simple_batch)
    {

        FTimespan const timeout{0, 0, 3};
        TestCommandBuilder.StartWhen([this] { return nullptr != spawner->FindFirstPlayerPawn(); })
            .Then([this] {
                auto& world{spawner->GetWorld()};

                for (TActorIterator<ATestBatchOrchestrator> it(&world); it; ++it) {
                    orchestrator = *it;
                    break;
                }
                ASSERT_THAT(IsNotNull(orchestrator));

                registry = orchestrator->get_entity_registry();
                ASSERT_THAT(IsNotNull(registry));

                turrets = orchestrator->get_turrets();
                ASSERT_THAT(IsNotNull(turrets));
            })
            .Until([&]() -> bool { return test_body(); }, timeout);
    }
};
