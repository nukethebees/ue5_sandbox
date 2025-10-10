#include "Sandbox/actor_components/RotatingActorComponent.h"

#include "Sandbox/subsystems/world/DestructionManagerSubsystem.h"
#include "Sandbox/subsystems/world/RotationManagerSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

URotatingActorComponent::URotatingActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void URotatingActorComponent::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(owner, GetOwner());
    TRY_INIT_PTR(rotation_manager, world->GetSubsystem<URotationManagerSubsystem>());
    TRY_INIT_PTR(root, owner->GetRootComponent());

    register_scene_component(*rotation_manager, *root);

    if (destroy_component_after_static_registration) {
        TRY_INIT_PTR(destruction_manager, world->GetSubsystem<UDestructionManagerSubsystem>());
        destruction_manager->queue_destruction(this);
    }
}

void URotatingActorComponent::EndPlay(EEndPlayReason::Type EndPlayReason) {
    if (!destroy_component_after_static_registration) {
        unregister_from_subsystem();
    }

    Super::EndPlay(EndPlayReason);
}

void URotatingActorComponent::unregister_from_subsystem() {
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(rotation_manager, world->GetSubsystem<URotationManagerSubsystem>());

    rotation_manager->remove(*this);
}
void URotatingActorComponent::register_scene_component(URotationManagerSubsystem& rotation_manager,
                                                       USceneComponent& scene_component) {
    if (rotation_type == ERotationType::STATIC) {
        rotation_manager.add(scene_component, speed);
    } else {
        rotation_manager.add(scene_component, *this);
    }
}
