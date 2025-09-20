#include "Sandbox/actors/JumpUpgradePickup.h"

#include "Sandbox/actor_components/RotatingActorComponent.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

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

    try_add_subsystem_payload<UCollisionEffectSubsystem>(this,
                                                         FJumpIncreasePayload(jump_count_increase));
}
