#include <SandboxCoreEngine/actor_components.h>
#include <SandboxCoreEngine/collision_settings.h>

#include <Components/BoxComponent.h>
#include <Components/MapTestSpawner.h>
#include <Components/SceneComponent.h>
#include <CQTest.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>

TEST_CLASS(CollisionSettings, "SandboxCoreEngine.UnitTests")
{
    TEST_METHOD(CopyAndApplyRoundTripsEverySetting)
    {
        auto* const component{NewObject<UBoxComponent>()};
        component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        component->SetCollisionObjectType(ECC_GameTraceChannel2);
        component->SetCollisionResponseToAllChannels(ECR_Ignore);
        component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        component->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        component->SetGenerateOverlapEvents(true);
        component->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_Yes;
        component->SetNotifyRigidBodyCollision(true);

        auto const settings{ml::copy_collision_settings(*component)};

        component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        component->SetCollisionObjectType(ECC_WorldDynamic);
        component->SetCollisionResponseToAllChannels(ECR_Block);
        component->SetGenerateOverlapEvents(false);
        component->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
        component->SetNotifyRigidBodyCollision(false);
        ml::apply_collision_settings(*component, settings);

        TestRunner->TestEqual(TEXT("Collision enabled is restored"),
                              component->GetCollisionEnabled(),
                              ECollisionEnabled::QueryAndPhysics);
        TestRunner->TestEqual(TEXT("Object type is restored"),
                              component->GetCollisionObjectType(),
                              ECollisionChannel::ECC_GameTraceChannel2);
        TestRunner->TestEqual(TEXT("Pawn response is restored"),
                              component->GetCollisionResponseToChannel(ECC_Pawn),
                              ECollisionResponse::ECR_Overlap);
        TestRunner->TestEqual(TEXT("World static response is restored"),
                              component->GetCollisionResponseToChannel(ECC_WorldStatic),
                              ECollisionResponse::ECR_Block);
        TestRunner->TestEqual(TEXT("Unchanged channel response is restored"),
                              component->GetCollisionResponseToChannel(ECC_Camera),
                              ECollisionResponse::ECR_Ignore);
        TestRunner->TestTrue(TEXT("Overlap events are restored"),
                             component->GetGenerateOverlapEvents());
        TestRunner->TestEqual(TEXT("Step-up setting is restored"),
                              component->CanCharacterStepUpOn.GetValue(),
                              ECanBeCharacterBase::ECB_Yes);
        TestRunner->TestTrue(TEXT("Rigid-body notifications are restored"),
                             component->BodyInstance.bNotifyRigidBodyCollision);
    }
};

TEST_CLASS(ActorComponents, "SandboxCoreEngine.LevelTests")
{
    TUniquePtr<FMapTestSpawner> spawner{nullptr};

    BEFORE_EACH()
    {
        spawner = FMapTestSpawner::CreateFromTempLevel(TestCommandBuilder);
        spawner->AddWaitUntilLoadedCommand(TestRunner);
    }

    AFTER_EACH()
    { spawner.Reset(); }

    TEST_METHOD(CreateAttachedComponentConfiguresOwnershipAndRegistration)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            auto* const actor{world.SpawnActor<AActor>()};
            auto* const root{NewObject<USceneComponent>(actor, TEXT("Root"))};
            actor->SetRootComponent(root);
            actor->AddInstanceComponent(root);
            root->RegisterComponent();

            auto* const child{ml::create_attached_instance_component<USceneComponent>(
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
            auto& world{spawner->GetWorld()};
            auto* const actor{world.SpawnActor<AActor>()};
            auto* const root{NewObject<USceneComponent>(actor, TEXT("Root"))};
            actor->SetRootComponent(root);
            actor->AddInstanceComponent(root);
            root->RegisterComponent();

            TArray<USceneComponent*> components{
                ml::create_attached_instance_component<USceneComponent>(
                    *actor, TEXT("First"), *root),
                ml::create_attached_instance_component<USceneComponent>(
                    *actor, TEXT("Second"), *root),
                ml::create_attached_instance_component<USceneComponent>(
                    *actor, TEXT("Third"), *root),
            };
            auto* const first{components[0]};
            auto* const second{components[1]};
            auto* const third{components[2]};
            third->DestroyComponent();
            components[2] = nullptr;

            ml::destroy_components_at_array_end(components, 2);

            TestRunner->TestEqual(TEXT("Only the leading component remains"), components.Num(), 1);
            TestRunner->TestTrue(TEXT("Leading component remains registered"),
                                 first->IsRegistered());
            TestRunner->TestFalse(TEXT("Second component is unregistered"), second->IsRegistered());
            TestRunner->TestFalse(TEXT("Third component is unregistered"), third->IsRegistered());

            components.Add(nullptr);
            ml::destroy_components_array(components);
            TestRunner->TestTrue(TEXT("All component entries are removed"), components.IsEmpty());
            TestRunner->TestFalse(TEXT("Leading component is destroyed"), first->IsRegistered());
        });
    }

    TEST_METHOD(RegisterComponentsRegistersValidEntriesAndSkipsNull)
    {
        TestCommandBuilder.Do([this] {
            auto& world{spawner->GetWorld()};
            auto* const actor{world.SpawnActor<AActor>()};
            auto* const first{NewObject<USceneComponent>(actor, TEXT("First"))};
            auto* const second{NewObject<USceneComponent>(actor, TEXT("Second"))};
            USceneComponent* const missing{nullptr};

            ml::register_components(first, missing, second);

            TestRunner->TestTrue(TEXT("First component is registered"), first->IsRegistered());
            TestRunner->TestTrue(TEXT("Second component is registered"), second->IsRegistered());
        });
    }
};
