#include "Sandbox/items/ShipLifeItem.h"

#include "Sandbox/environment/effects/RotatingActorComponent.h"
#include "Sandbox/players/SpaceShip.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AShipLifeItem::AShipLifeItem()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box")))
    , rotator(CreateDefaultSubobject<URotatingActorComponent>(TEXT("rotator"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);
}

void AShipLifeItem::BeginPlay() {
    Super::BeginPlay();

    collision_box->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::on_overlap_begin);
}

void AShipLifeItem::on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                                     AActor* other_actor,
                                     UPrimitiveComponent* other_comp,
                                     int32 other_body_index,
                                     bool from_sweep,
                                     FHitResult const& sweep_result) {
    if (auto* ship{Cast<ASpaceShip>(other_actor)}) {
        ship->add_life();
        Destroy();
    }
}
