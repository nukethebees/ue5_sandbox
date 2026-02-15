#include "Sandbox/players/npcs/ShipTrainingTarget.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AShipTrainingTarget::AShipTrainingTarget()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

auto AShipTrainingTarget::apply_damage(int32 damage, AActor const& instigator)
    -> FShipDamageResult {
    SetLifeSpan(0.01f);
    return FShipDamageResult{EDamageResult::Killed};
}
