#include "Sandbox/environment/effects/actor_components/RotatingActorComponent.h"

#include "Engine/World.h"

#include "Sandbox/core/destruction/subsystems/DestructionManagerSubsystem.h"
#include "Sandbox/environment/effects/subsystems/RotationManagerSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

URotatingActorComponent::URotatingActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void URotatingActorComponent::BeginPlay() {
    Super::BeginPlay();

    if (autoregster_parent_root) {
        TRY_INIT_PTR(owner, GetOwner());
        TRY_INIT_PTR(root, owner->GetRootComponent());
        register_scene_component(*root);
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
                                                       UWorld& world,
                                                       USceneComponent& scene_component) {
    if (rotation_type == ERotationType::STATIC) {
        rotation_manager.add(scene_component, speed);
    } else {
        rotation_manager.add(scene_component, *this);
    }

    if (destroy_component_after_static_registration) {
        TRY_INIT_PTR(destruction_manager, world.GetSubsystem<UDestructionManagerSubsystem>());
        destruction_manager->queue_destruction(this);
    }
}
void URotatingActorComponent::register_scene_component(USceneComponent& scene_component) {
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(rotation_manager, world->GetSubsystem<URotationManagerSubsystem>());

    register_scene_component(*rotation_manager, *world, scene_component);
}
