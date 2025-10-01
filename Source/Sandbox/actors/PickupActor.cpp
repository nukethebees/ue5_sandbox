#include "Sandbox/actors/PickupActor.h"

#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

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
void APickupActor::BeginPlay() {
    Super::BeginPlay();
}
