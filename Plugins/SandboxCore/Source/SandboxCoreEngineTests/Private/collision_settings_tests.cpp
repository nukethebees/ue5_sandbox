#include <SandboxCoreEngine/collision_settings.h>

#include <Components/BoxComponent.h>
#include <CQTest.h>

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

        auto const settings{ioj::copy_collision_settings(*component)};

        component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        component->SetCollisionObjectType(ECC_WorldDynamic);
        component->SetCollisionResponseToAllChannels(ECR_Block);
        component->SetGenerateOverlapEvents(false);
        component->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
        component->SetNotifyRigidBodyCollision(false);
        ioj::apply_collision_settings(*component, settings);

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
