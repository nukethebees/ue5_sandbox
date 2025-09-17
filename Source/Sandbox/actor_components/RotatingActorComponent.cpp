#include "Sandbox/actor_components/RotatingActorComponent.h"
#include "Sandbox/subsystems/RotationManagerSubsystem.h"

URotatingActorComponent::URotatingActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void URotatingActorComponent::BeginPlay() {
    Super::BeginPlay();

    if (auto* world{GetWorld()}) {
        if (auto* rotation_manager{world->GetSubsystem<URotationManagerSubsystem>()}) {
            rotation_manager->register_rotating_actor(this, GetOwner(), rotation_type);
        }
    }

    if (destroy_component_after_static_registration) {
        DestroyComponent();
    }
}

void URotatingActorComponent::EndPlay(EEndPlayReason::Type EndPlayReason) {
    if (auto* world{GetWorld()}) {
        if (auto* rotation_manager{world->GetSubsystem<URotationManagerSubsystem>()}) {
            rotation_manager->unregister_rotating_actor(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}
