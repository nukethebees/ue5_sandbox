#include "Sandbox/items/ShipTrainingRing.h"

#include "Sandbox/environment/effects/RotatingActorComponent.h"
#include "Sandbox/items/ShipHealthItemConfig.h"
#include "Sandbox/players/ShipScoringSubsystem.h"
#include "Sandbox/players/SpaceShip.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipTrainingRing::AShipTrainingRing()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box")))
    , rotator(CreateDefaultSubobject<URotatingActorComponent>(TEXT("rotator"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);
}

void AShipTrainingRing::BeginPlay() {
    Super::BeginPlay();

    collision_box->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::on_overlap_begin);
}
void AShipTrainingRing::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
}

void AShipTrainingRing::on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                                         AActor* other_actor,
                                         UPrimitiveComponent* other_comp,
                                         int32 other_body_index,
                                         bool from_sweep,
                                         FHitResult const& sweep_result) {
    if (auto* ship{Cast<ASpaceShip>(other_actor)}) {
        TRY_INIT_PTR(world, GetWorld());
        TRY_INIT_PTR(ss, world->GetSubsystem<UShipScoringSubsystem>());
        ss->register_points(*ship, 1);

        if (shockwave) {
            auto const ring_loc{GetActorLocation()};
            auto const ship_loc{ship->GetActorLocation()};
            auto const ship_fwd{ship->GetActorForwardVector()};
            auto const ship_up{ship->GetActorUpVector()};

            auto const d_fwd{ship_fwd * shockwave_ship_fwd};
            auto const d_up{ship_up * shockwave_ship_up};
            auto const loc{ship_loc + d_fwd + d_up};

            UNiagaraFunctionLibrary::SpawnSystemAtLocation(world,
                                                           shockwave,
                                                           loc,
                                                           FRotator::ZeroRotator,
                                                           FVector(1.0f),
                                                           true,
                                                           true,
                                                           ENCPoolMethod::None,
                                                           true);
        }

        Destroy();
    }
}
