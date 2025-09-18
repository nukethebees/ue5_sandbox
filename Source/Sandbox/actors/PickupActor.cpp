#include "Sandbox/actors/PickupActor.h"

#include "Sandbox/utilities/CollisionEffectHelpers.h"

APickupActor::APickupActor() {
    PrimaryActorTick.bCanEverTick = false;

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
    RootComponent = collision_component;
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    collision_component->SetBoxExtent(FVector{50.0f, 50.0f, 50.0f});
}
void APickupActor::BeginPlay() {
    Super::BeginPlay();

    CollisionEffectHelpers::register_with_collision_system(this);
}
