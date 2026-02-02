#include "Sandbox/items/PickupActor.h"

#include "Components/BoxComponent.h"

#include "Sandbox/interaction/CollisionEffectSubsystem.h"

APickupActor::APickupActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
    collision_component->SetupAttachment(RootComponent);
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    collision_component->SetBoxExtent(FVector{50.0f, 50.0f, 50.0f});
}

UPrimitiveComponent* APickupActor::get_collision_component() {
    return collision_component;
}
bool APickupActor::should_destroy_after_collision() const {
    return destroy_after_collision;
}
void APickupActor::BeginPlay() {
    Super::BeginPlay();
}
