#include "Sandbox/actors/SpeedBoostItemActor.h"

#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

ASpeedBoostItemActor::ASpeedBoostItemActor() {
    // Create mesh component for visual representation
    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    speed_boost_component =
        CreateDefaultSubobject<USpeedBoostItemComponent>(TEXT("SpeedBoostComponent"));
}
