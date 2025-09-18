#include "Sandbox/actors/PickupActor.h"

APickupActor::APickupActor() {
    PrimaryActorTick.bCanEverTick = false;

    pickup_component = CreateDefaultSubobject<UPickupComponent>(TEXT("PickupComponent"));

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
    RootComponent = collision_component;
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    collision_component->SetBoxExtent(FVector{50.0f, 50.0f, 50.0f});
}

UPrimitiveComponent* APickupActor::get_pickup_collision_component() {
    return collision_component;
}

bool APickupActor::should_destroy_after_pickup() const {
    return destroy_after_pickup;
}

float APickupActor::get_pickup_cooldown() const {
    return pickup_cooldown;
}
