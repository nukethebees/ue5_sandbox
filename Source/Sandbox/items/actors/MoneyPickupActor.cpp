#include "Sandbox/items/actors/MoneyPickupActor.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/interaction/triggering/subsystems/TriggerSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

AMoneyPickupActor::AMoneyPickupActor() {
    PrimaryActorTick.bCanEverTick = false;

    constexpr auto mobility{EComponentMobility::Movable};

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(mobility);

    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    mesh_component->SetMobility(mobility);

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
    collision_component->SetupAttachment(RootComponent);
    collision_component->SetMobility(mobility);
    collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_component->SetCollisionResponseToChannel(ml::collision::interaction, ECR_Block);
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AMoneyPickupActor::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(manager, world->GetSubsystem<UTriggerSubsystem>());

    manager->register_triggerable(*this, money_payload);
}
