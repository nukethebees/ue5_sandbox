#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>

TEST_CLASS(SpawnActors, "SandboxCoreEngine.LevelTests")
{
    static constexpr int32 actor_count{3};

    TUniquePtr<FMapTestSpawner> spawner{nullptr};

    BEFORE_EACH()
    {
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    {
        spawner.Reset();
    }

    TEST_METHOD(SpawnsAndInitialisesBatch)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            TArray<AActor*> actors;
            actors.Init(nullptr, actor_count);

            auto initialise_call_count{0};
            ml::spawn_actors<AActor>(
                world,
                AActor::StaticClass(),
                actors,
                [this, &initialise_call_count](TArrayView<AActor*> configured_actors) {
                    ++initialise_call_count;
                    TestRunner->TestEqual(
                        TEXT("Initialise receives every actor"), configured_actors.Num(), actor_count);

                    const auto configured_actor_count{configured_actors.Num()};
                    for (auto actor_index{0}; actor_index < configured_actor_count; ++actor_index) {
                        auto* const actor{configured_actors[actor_index]};
                        if (!TestRunner->TestNotNull(TEXT("Deferred actor is available"), actor)) {
                            continue;
                        }

                        actor->Tags.Add(
                            FName{FString::Printf(TEXT("spawn_actors_test_%d"), actor_index)});
                    }
                });

            TestRunner->TestEqual(
                TEXT("Initialise is called once for the batch"), initialise_call_count, 1);

            TSet<AActor*> unique_actors;
            const auto spawned_actor_count{actors.Num()};
            for (auto actor_index{0}; actor_index < spawned_actor_count; ++actor_index) {
                auto* const actor{actors[actor_index]};
                if (!TestRunner->TestTrue(TEXT("Spawned actor remains valid"), IsValid(actor))) {
                    continue;
                }

                unique_actors.Add(actor);
                TestRunner->TestTrue(
                    TEXT("Spawned actor retains its initialised tag"),
                    actor->Tags.Contains(FName{FString::Printf(TEXT("spawn_actors_test_%d"), actor_index)}));
            }

            TestRunner->TestEqual(
                TEXT("Every output slot contains a distinct actor"), unique_actors.Num(), actor_count);
        });
    }
};
