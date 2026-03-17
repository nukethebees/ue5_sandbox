#include "Sandbox/players/ShipTrainingTarget.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipTrainingTarget::AShipTrainingTarget()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    mesh->SetMobility(EComponentMobility::Movable);
    mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    collision_box->SetupAttachment(RootComponent);
    collision_box->SetMobility(EComponentMobility::Movable);
    collision_box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_box->SetCollisionResponseToChannel(ml::collision::projectile,
                                                 ECollisionResponse::ECR_Block);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AShipTrainingTarget::Tick(float dt) {
    Super::Tick(dt);

    switch (movement_mode) {
        using enum EShipTrainingTargetMovementMode;
        case move_forward: {
            SetActorLocation(GetActorLocation() + GetActorForwardVector() * velocity);
            break;
        }
        default: {
            UE_LOG(LogSandboxActor, Warning, TEXT("AShipTrainingTarget: Unhandled enum"));
            [[fallthrough]];
        }
        case stationary: {
            break;
        }
    }
}

auto AShipTrainingTarget::apply_damage(int32 damage, AActor const& instigator)
    -> FShipDamageResult {
    SetLifeSpan(0.01f);
    return FShipDamageResult{EDamageResult::Killed};
}

void AShipTrainingTarget::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    TRY_INIT_PTR(tgt_mesh, mesh->GetStaticMesh());
    auto const bbox{tgt_mesh->GetBoundingBox()};
    collision_box->SetBoxExtent(bbox.GetExtent());
}
