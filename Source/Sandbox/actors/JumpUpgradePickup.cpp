#include "Sandbox/actors/JumpUpgradePickup.h"

#include "Sandbox/actor_components/RotatingActorComponent.h"
#include "Sandbox/characters/MyCharacter.h"
#include "Sandbox/subsystems/CollisionEffectSubsystem2.h"

AJumpUpgradePickup::AJumpUpgradePickup() {
    // Create mesh component for visual representation
    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create rotating component for visual effect
    rotating_component = CreateDefaultSubobject<URotatingActorComponent>(TEXT("RotatingComponent"));
}

void AJumpUpgradePickup::on_collision_effect(AActor* other_actor) {
    if (!other_actor) {
        return;
    }

    if (auto* character{Cast<AMyCharacter>(other_actor)}) {
        character->increase_max_jump_count(jump_count_increase);
    }
}

void AJumpUpgradePickup::BeginPlay() {
    Super::BeginPlay();

    // Register with the new subsystem
    if (auto* world{GetWorld()}) {
        if (auto* subsystem{world->GetSubsystem<UCollisionEffectSubsystem2>()}) {
            subsystem->add_payload(this, FJumpIncreasePayload(jump_count_increase));
        }
    }
}
