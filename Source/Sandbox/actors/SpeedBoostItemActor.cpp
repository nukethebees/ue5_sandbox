#include "Sandbox/actors/SpeedBoostItemActor.h"

#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

ASpeedBoostItemActor::ASpeedBoostItemActor() {
    PrimaryActorTick.bCanEverTick = false;

    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = mesh_component;
    mesh_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    mesh_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    mesh_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    speed_boost_component =
        CreateDefaultSubobject<USpeedBoostItemComponent>(TEXT("SpeedBoostComponent"));
}

UPrimitiveComponent* ASpeedBoostItemActor::get_pickup_collision_component() {
    return mesh_component;
}

bool ASpeedBoostItemActor::should_destroy_after_pickup() const {
    return destroy_after_pickup;
}

float ASpeedBoostItemActor::get_pickup_cooldown() const {
    return pickup_cooldown;
}

void ASpeedBoostItemActor::on_pre_pickup_effect(AActor* collector) {
    // Actor can add visual/audio effects here
}

void ASpeedBoostItemActor::on_post_pickup_effect(AActor* collector) {
    // Actor can add cleanup effects here
}
