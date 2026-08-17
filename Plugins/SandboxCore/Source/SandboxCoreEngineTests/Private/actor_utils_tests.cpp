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
    { spawner.Reset(); }

    TEST_METHOD(SpawnsAndInitialisesBatch)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            TArray<AActor*> actors;
            actors.Init(nullptr, actor_count);

            auto pre_spawn_call_count{0};
            auto post_spawn_call_count{0};
            ml::spawn_actors<AActor>(
                world,
                AActor::StaticClass(),
                actors,
                [this, &pre_spawn_call_count, &post_spawn_call_count](
                    TArrayView<AActor*> configured_actors, ESpawnPhase const phase) {
                    if (phase == ESpawnPhase::PostSpawn) {
                        ++post_spawn_call_count;
                        return;
                    }

                    ++pre_spawn_call_count;
                    TestRunner->TestEqual(TEXT("Initialise receives every actor"),
                                          configured_actors.Num(),
                                          actor_count);

                    auto const configured_actor_count{configured_actors.Num()};
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
                TEXT("Pre-spawn callback is called once for the batch"), pre_spawn_call_count, 1);
            TestRunner->TestEqual(
                TEXT("Post-spawn callback is called once for the batch"), post_spawn_call_count, 1);

            TSet<AActor*> unique_actors;
            auto const spawned_actor_count{actors.Num()};
            for (auto actor_index{0}; actor_index < spawned_actor_count; ++actor_index) {
                auto* const actor{actors[actor_index]};
                if (!TestRunner->TestTrue(TEXT("Spawned actor remains valid"), IsValid(actor))) {
                    continue;
                }

                unique_actors.Add(actor);
                TestRunner->TestTrue(TEXT("Spawned actor retains its initialised tag"),
                                     actor->Tags.Contains(FName{FString::Printf(
                                         TEXT("spawn_actors_test_%d"), actor_index)}));
            }

            TestRunner->TestEqual(TEXT("Every output slot contains a distinct actor"),
                                  unique_actors.Num(),
                                  actor_count);
        });
    }

    TEST_METHOD(PreservesInitialisedActorLocations)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            TArray<AActor*> actors;
            actors.Init(nullptr, actor_count);

            TArray<FVector> const expected_locations{FVector{100.f, 200.f, 300.f},
                                                     FVector{-400.f, 500.f, -600.f},
                                                     FVector{700.f, -800.f, 900.f}};

            ml::spawn_actors<AActor>(
                world,
                actors,
                [&expected_locations](TArrayView<AActor*> actors, ESpawnPhase const phase) {
                    if (phase == ESpawnPhase::PreSpawn) {
                        for (auto* actor : actors) {
                            auto* root = NewObject<USceneComponent>(actor);
                            actor->AddInstanceComponent(root);
                            actor->SetRootComponent(root);
                            root->RegisterComponent();
                        }
                        return;
                    }

                    auto const actor_count{actors.Num()};
                    for (auto actor_index{0}; actor_index < actor_count; ++actor_index) {
                        actors[actor_index]->SetActorLocation(expected_locations[actor_index]);
                    }
                });

            auto const spawned_actor_count{actors.Num()};
            for (auto actor_index{0}; actor_index < spawned_actor_count; ++actor_index) {
                auto* const actor{actors[actor_index]};
                if (!TestRunner->TestNotNull(TEXT("Spawned actor is available"), actor)) {
                    continue;
                }

                TestRunner->TestTrue(
                    TEXT("Spawned actor retains its initialised location"),
                    actor->GetActorLocation().Equals(expected_locations[actor_index]));
            }
        });
    }
};
