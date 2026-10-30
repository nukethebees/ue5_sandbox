#include <SandboxCoreEngine/actor_components.h>
#include <SandboxCoreEngine/actor_utils.h>

#include <Components/MapTestSpawner.h>
#include <Components/SceneComponent.h>
#include <Containers/Set.h>
#include <CQTest.h>
#include <Editor.h>
#include <Editor/UnrealEdEngine.h>
#include <Engine/TriggerBox.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <HAL/FileManager.h>
#include <LevelEditorSubsystem.h>
#include <Misc/Guid.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>
#include <Tests/AutomationEditorCommon.h>
#include <UnrealEdGlobals.h>

TEST_CLASS(ActorsAndComponents, "SandboxCoreEngine.LevelTests")
{
    static constexpr int32 actor_count{3};

    /* **************************************** */
    // Fixture lifecycle
    /* **************************************** */
    BEFORE_EACH()
    {
        if (!spawner_) {
            if (IsValid(GUnrealEd->PlayWorld)) {
                GUnrealEd->EndPlayMap();
            }

            map_directory_ = FPaths::Combine(FPaths::ProjectContentDir(),
                                             TEXT("SandboxCoreReusableTestLevels"),
                                             FGuid::NewGuid().ToString(EGuidFormats::Digits));
            auto const map_name{TEXT("level")};
            auto const package_name{
                FPackageName::FilenameToLongPackageName(FPaths::Combine(map_directory_, map_name))};
            auto* const level_editor{GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()};
            if (!TestRunner->TestTrue(TEXT("Shared blank level is created"),
                                      level_editor->NewLevel(package_name))) {
                return;
            }

            spawner_ = MakeUnique<FMapTestSpawner>(map_directory_, map_name);
            spawner_->AddWaitUntilLoadedCommand(TestRunner);
            TestCommandBuilder.Do([] {
                world_ = &spawner_->GetWorld();
                for (auto* const actor : ioj::get_actors<AActor>(*world_)) {
                    baseline_actors_.Add(actor);
                }
            });
        }

        TestCommandBuilder.Do([this] {
            TestRunner->TestEqual(TEXT("Each test starts with only the map's framework actors"),
                                  ioj::count_actors<AActor>(*world_),
                                  baseline_actors_.Num());
        });
    }
    AFTER_EACH()
    {
        if (!world_.IsValid()) {
            TestRunner->AddError(TEXT("Shared test world is unavailable during cleanup"));
            return;
        }

        auto const actors{ioj::get_actors<AActor>(*world_)};
        for (auto* const actor : actors) {
            if (!baseline_actors_.Contains(actor)) {
                TestRunner->TestTrue(TEXT("Test-created actor is destroyed"), actor->Destroy());
            }
        }
    }
    AFTER_ALL()
    {
        if (IsValid(GUnrealEd->PlayWorld)) {
            GUnrealEd->EndPlayMap();
        }

        FAutomationEditorCommonUtils::CreateNewMap();
        spawner_.Reset();
        world_.Reset();
        baseline_actors_.Reset();

        if (!map_directory_.IsEmpty()) {
            if (!IFileManager::Get().DeleteDirectory(*map_directory_, false, true)) {
                UE_LOG(
                    LogTemp, Error, TEXT("Failed to delete shared test map: %s"), *map_directory_);
            }
            map_directory_.Reset();
        }
    }

    /* **************************************** */
    // Actor utilities
    /* **************************************** */
    TEST_METHOD(SpawnsAndInitialisesBatch)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner_->GetWorld()};
            TArray<AActor*> actors;
            actors.Init(nullptr, actor_count);

            auto pre_spawn_call_count{0};
            auto post_spawn_call_count{0};
            ioj::spawn_actors<AActor>(
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
            auto& world{spawner_->GetWorld()};
            TArray<AActor*> actors;
            actors.Init(nullptr, actor_count);

            TArray<FVector> const expected_locations{FVector{100.f, 200.f, 300.f},
                                                     FVector{-400.f, 500.f, -600.f},
                                                     FVector{700.f, -800.f, 900.f}};
            auto pre_spawn_callback_count{0};
            auto post_spawn_callback_count{0};

            ioj::spawn_actors<AActor>(
                world,
                actors,
                [this,
                 &actors,
                 &expected_locations,
                 &pre_spawn_callback_count,
                 &post_spawn_callback_count](
                    AActor& actor, int32 const actor_index, ESpawnPhase const phase) {
                    if (!TestRunner->TestTrue(TEXT("Callback index is a valid output slot"),
                                              actors.IsValidIndex(actor_index))) {
                        return;
                    }
                    TestRunner->TestTrue(TEXT("Callback actor matches its output slot"),
                                         &actor == actors[actor_index]);

                    if (phase == ESpawnPhase::PreSpawn) {
                        ++pre_spawn_callback_count;
                        TestRunner->TestFalse(TEXT("Deferred actor is not initialized"),
                                              actor.IsActorInitialized());

                        auto* const root{NewObject<USceneComponent>(&actor)};
                        actor.AddInstanceComponent(root);
                        actor.SetRootComponent(root);
                        root->RegisterComponent();
                        actor.Tags.Add(
                            FName{FString::Printf(TEXT("pre_spawn_actor_%d"), actor_index)});
                        return;
                    }

                    ++post_spawn_callback_count;
                    TestRunner->TestTrue(TEXT("Finished actor is initialized"),
                                         actor.IsActorInitialized());
                    TestRunner->TestTrue(
                        TEXT("Post-spawn callback observes pre-spawn configuration"),
                        actor.Tags.Contains(
                            FName{FString::Printf(TEXT("pre_spawn_actor_%d"), actor_index)}));
                    actor.SetActorLocation(expected_locations[actor_index]);
                });

            TestRunner->TestEqual(TEXT("Pre-spawn callback runs once per actor"),
                                  pre_spawn_callback_count,
                                  actor_count);
            TestRunner->TestEqual(TEXT("Post-spawn callback runs once per actor"),
                                  post_spawn_callback_count,
                                  actor_count);

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

    TEST_METHOD(ActorQueriesTrackSpawnedAndDestroyedActors)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner_->GetWorld()};
            auto const initial_count{ioj::count_actors<ATriggerBox>(world)};
            auto* const first{world.SpawnActor<ATriggerBox>()};
            auto* const second{world.SpawnActor<ATriggerBox>()};

            TestRunner->TestEqual(TEXT("Count includes both spawned actors"),
                                  ioj::count_actors<ATriggerBox>(world),
                                  initial_count + 2);
            TestRunner->TestTrue(TEXT("First actor is in its world"),
                                 ioj::is_actor_in_world(world, first));
            TestRunner->TestFalse(TEXT("Null actor is not in the world"),
                                  ioj::is_actor_in_world(world, nullptr));
            auto* const actor_without_world{NewObject<AActor>()};
            TestRunner->TestFalse(TEXT("Actor outside the world is rejected"),
                                  ioj::is_actor_in_world(world, actor_without_world));
            TestRunner->TestNotNull(TEXT("First actor query returns an actor"),
                                    ioj::get_first_actor<ATriggerBox>(world));

            auto actors{ioj::get_actors<ATriggerBox>(world)};
            TestRunner->TestEqual(
                TEXT("Get actors matches count"), actors.Num(), initial_count + 2);
            TestRunner->TestTrue(TEXT("Get actors includes first"), actors.Contains(first));
            TestRunner->TestTrue(TEXT("Get actors includes second"), actors.Contains(second));

            TArray<ATriggerBox*> appended{first};
            ioj::append_actors(world, appended);
            TestRunner->TestEqual(TEXT("Append actors preserves existing entries"),
                                  appended.Num(),
                                  initial_count + 3);
            TestRunner->TestTrue(TEXT("Existing entry remains first"), appended[0] == first);

            int32 visit_count{};
            ioj::for_each_instance(*first, [&visit_count](ATriggerBox&) { ++visit_count; });
            TestRunner->TestEqual(
                TEXT("For each visits every instance"), visit_count, initial_count + 2);

            second->Destroy();
            TestRunner->TestFalse(TEXT("Destroyed actor is no longer in the world"),
                                  ioj::is_actor_in_world(world, second));
        });
    }

    TEST_METHOD(GetOrCreateSingletonReusesExistingActor)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner_->GetWorld()};
            auto const initial_count{ioj::count_actors<ATriggerBox>(world)};
            auto* const first{ioj::get_or_create_actor_singleton<ATriggerBox>(world)};
            auto* const second{ioj::get_or_create_actor_singleton<ATriggerBox>(world)};

            TestRunner->TestNotNull(TEXT("Singleton is available"), first);
            TestRunner->TestTrue(TEXT("Singleton call reuses the same actor"), first == second);
            TestRunner->TestEqual(TEXT("Only one singleton actor is created"),
                                  ioj::count_actors<ATriggerBox>(world),
                                  initial_count + 1);
        });
    }

    /* **************************************** */
    // Actor components
    /* **************************************** */
    TEST_METHOD(CreateAttachedComponentConfiguresOwnershipAndRegistration)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner_->GetWorld()};
            auto* const actor{world.SpawnActor<AActor>()};
            auto* const root{NewObject<USceneComponent>(actor, TEXT("Root"))};
            actor->SetRootComponent(root);
            actor->AddInstanceComponent(root);
            root->RegisterComponent();

            auto* const child{ioj::create_attached_instance_component<USceneComponent>(
                *actor, TEXT("GeneratedChild"), *root)};

            TestRunner->TestNotNull(TEXT("Component is created"), child);
            TestRunner->TestTrue(TEXT("Component has the requested name"),
                                 child->GetFName() == TEXT("GeneratedChild"));
            TestRunner->TestTrue(TEXT("Actor owns the component"), child->GetOwner() == actor);
            TestRunner->TestTrue(TEXT("Component is registered"), child->IsRegistered());
            TestRunner->TestTrue(TEXT("Component is attached to the parent"),
                                 child->GetAttachParent() == root);
            TestRunner->TestTrue(TEXT("Component is an instance component"),
                                 actor->GetInstanceComponents().Contains(child));
        });
    }

    TEST_METHOD(DestroyHelpersRemoveOnlyRequestedComponents)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner_->GetWorld()};
            auto* const actor{world.SpawnActor<AActor>()};
            auto* const root{NewObject<USceneComponent>(actor, TEXT("Root"))};
            actor->SetRootComponent(root);
            actor->AddInstanceComponent(root);
            root->RegisterComponent();

            TArray<USceneComponent*> components{
                ioj::create_attached_instance_component<USceneComponent>(
                    *actor, TEXT("First"), *root),
                ioj::create_attached_instance_component<USceneComponent>(
                    *actor, TEXT("Second"), *root),
                ioj::create_attached_instance_component<USceneComponent>(
                    *actor, TEXT("Third"), *root),
            };
            auto* const first{components[0]};
            auto* const second{components[1]};
            auto* const third{components[2]};
            third->DestroyComponent();
            components[2] = nullptr;

            ioj::destroy_components_at_array_end(components, 2);

            TestRunner->TestEqual(TEXT("Only the leading component remains"), components.Num(), 1);
            TestRunner->TestTrue(TEXT("Leading component remains registered"),
                                 first->IsRegistered());
            TestRunner->TestFalse(TEXT("Second component is unregistered"), second->IsRegistered());
            TestRunner->TestFalse(TEXT("Third component is unregistered"), third->IsRegistered());

            components.Add(nullptr);
            ioj::destroy_components_array(components);
            TestRunner->TestTrue(TEXT("All component entries are removed"), components.IsEmpty());
            TestRunner->TestFalse(TEXT("Leading component is destroyed"), first->IsRegistered());
        });
    }

    TEST_METHOD(RegisterComponentsRegistersValidEntriesAndSkipsNull)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner_->GetWorld()};
            auto* const actor{world.SpawnActor<AActor>()};
            auto* const first{NewObject<USceneComponent>(actor, TEXT("First"))};
            auto* const second{NewObject<USceneComponent>(actor, TEXT("Second"))};
            USceneComponent* const missing{nullptr};

            ioj::register_components(first, missing, second);

            TestRunner->TestTrue(TEXT("First component is registered"), first->IsRegistered());
            TestRunner->TestTrue(TEXT("Second component is registered"), second->IsRegistered());
        });
    }
  private:
    inline static TUniquePtr<FMapTestSpawner> spawner_{};
    inline static TWeakObjectPtr<UWorld> world_{};
    inline static TSet<AActor*> baseline_actors_{};
    inline static FString map_directory_{};
};
