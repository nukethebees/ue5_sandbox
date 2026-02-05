#include "Sandbox/items/ShipHealthItem.h"

#include "Sandbox/environment/effects/RotatingActorComponent.h"
#include "Sandbox/items/ShipHealthItemConfig.h"
#include "Sandbox/players/playable/space_ship/SpaceShip.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AShipHealthItem::AShipHealthItem()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box")))
    , rotator(CreateDefaultSubobject<URotatingActorComponent>(TEXT("rotator"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);
}

void AShipHealthItem::set_config(UShipHealthItemConfig const& config) {
    material = config.material;
    health = config.health;
}

void AShipHealthItem::BeginPlay() {
    Super::BeginPlay();

    collision_box->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::on_overlap_begin);
}
void AShipHealthItem::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    RETURN_IF_NULLPTR(material);
    RETURN_IF_NULLPTR(mesh);

    mesh->SetMaterial(0, material);
}

void AShipHealthItem::on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                                       AActor* other_actor,
                                       UPrimitiveComponent* other_comp,
                                       int32 other_body_index,
                                       bool from_sweep,
                                       FHitResult const& sweep_result) {
    if (auto* ship{Cast<ASpaceShip>(other_actor)}) {
        if (type == EShipHealthItemType::Gold) {
            ship->add_gold_ring();
        }

        Destroy();
    }
}
