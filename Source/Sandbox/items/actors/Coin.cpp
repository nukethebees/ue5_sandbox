#include "Sandbox/items/actors/Coin.h"

#include "Sandbox/environment/effects/subsystems/RotationManagerSubsystem.h"
#include "Sandbox/interaction/collision/subsystems/CollisionEffectSubsystem.h"
#include "Sandbox/players/playable/actor_components/CoinCollectorActorComponent.h"
#include "Sandbox/players/playable/characters/MyCharacter.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ACoin::ACoin() {
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
}

void ACoin::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(rotation_manager, world->GetSubsystem<URotationManagerSubsystem>());
    RETURN_IF_NULLPTR(mesh_component);

    rotation_manager->add(*mesh_component, rotation_speed);

    TRY_INIT_PTR(collision_manager, world->GetSubsystem<UCollisionEffectSubsystem>());
    collision_manager->add_payload(*this, FCoinPayload(coin_value));
}
