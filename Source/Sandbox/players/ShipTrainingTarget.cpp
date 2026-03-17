#include "Sandbox/players/ShipTrainingTarget.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"

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

    TRY_INIT_PTR(world, GetWorld());

    auto const loc{GetActorLocation()};
    auto const fwd{GetActorForwardVector()};

    switch (movement_mode) {
        using enum EShipTrainingTargetMovementMode;
        case move_forward: {
            pos = loc + fwd * velocity;
            break;
        }
        case sine: {
            pos = loc + fwd * velocity;

            auto const t{world->GetTimeSeconds()};
            pos_offset = sine_amplitude * FMath::Sin(sine_frequency * t);
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

    SetActorLocation(pos + pos_offset);
}

auto AShipTrainingTarget::apply_damage(int32 damage, AActor const& instigator)
    -> FShipDamageResult {
    SetLifeSpan(0.01f);
    FShipDamageResult const result{EDamageResult::Killed};

    if (death_particles) {
        INIT_OR_RETURN_VALUE_IF_FALSE(auto*, world, GetWorld(), result);

        auto const loc{GetActorLocation()};
        auto const rot{GetActorRotation()};

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            world, death_particles, loc, rot, FVector::OneVector);
    }

    return result;
}

void AShipTrainingTarget::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    TRY_INIT_PTR(tgt_mesh, mesh->GetStaticMesh());
    auto const bbox{tgt_mesh->GetBoundingBox()};
    collision_box->SetBoxExtent(bbox.GetExtent());
}
void AShipTrainingTarget::BeginPlay() {
    Super::BeginPlay();
    pos = GetActorLocation();
}
