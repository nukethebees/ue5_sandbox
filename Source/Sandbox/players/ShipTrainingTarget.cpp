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
    collision_box->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
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

    auto const t{world->GetTimeSeconds()};
    wt = angular_frequency * t;
    if (wt >= 1.f) {
        wt -= 1.f;
    }
    auto const loc{GetActorLocation()};
    auto const fwd{GetActorForwardVector()};
    auto const d_speed{speed * dt};

    pos = loc + fwd * d_speed;

    switch (movement_mode) {
        using enum EShipTrainingTargetMovementMode;
        case move_forward: {
            break;
        }
        case sine: {
            pos_offset = sine_amplitude * FMath::Sin(wt);
            break;
        }
        case orbit_yz: {
            pos_offset = FVector3d{
                0.f,
                orbit_radius * FMath::Cos(wt),
                orbit_radius * FMath::Sin(wt),
            };
            break;
        }
        default: {
            UE_LOG(LogSandboxActor, Warning, TEXT("AShipTrainingTarget: Unhandled enum"));
            [[fallthrough]];
        }
        case stationary: {
            pos = GetActorLocation();
            pos_offset = FVector3d::ZeroVector;

            break;
        }
    }

    SetActorLocation(pos + pos_offset);
}

auto AShipTrainingTarget::apply_damage(ShipDamageContext context)
    -> FShipDamageResult {
    if (IsActorBeingDestroyed()) {
        return {EDamageResult::AlreadyDestroyed};
    }

    FShipDamageResult const result{EDamageResult::Killed};

    if (death_particles) {
        INIT_OR_RETURN_VALUE_IF_FALSE(auto*, world, GetWorld(), result);

        auto const loc{GetActorLocation()};
        auto const rot{GetActorRotation()};

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            world, death_particles, loc, rot, FVector::OneVector);
    }

    Destroy();
    return result;
}

void AShipTrainingTarget::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    TRY_INIT_PTR(tgt_mesh, mesh->GetStaticMesh());
    auto const bbox{tgt_mesh->GetBoundingBox()};
    collision_box->SetBoxExtent(bbox.GetExtent());
    pos = GetActorLocation();
    angular_frequency = 2.f * 3.14159f * frequency;
}
void AShipTrainingTarget::BeginPlay() {
    Super::BeginPlay();
}
