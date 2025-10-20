#include "Sandbox/items/actors/JumpUpgradePickup.h"

#include "Sandbox/environment/effects/actor_components/RotatingActorComponent.h"
#include "Sandbox/interaction/collision/subsystems/CollisionEffectSubsystem.h"
#include "Sandbox/players/playable/characters/MyCharacter.h"

AJumpUpgradePickup::AJumpUpgradePickup() {
    // Create mesh component for visual representation
    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create rotating component for visual effect
    rotating_component = CreateDefaultSubobject<URotatingActorComponent>(TEXT("RotatingComponent"));
}

void AJumpUpgradePickup::BeginPlay() {
    Super::BeginPlay();

    try_add_subsystem_payload<UCollisionEffectSubsystem>(*this,
                                                         FJumpIncreasePayload(jump_count_increase));
}
