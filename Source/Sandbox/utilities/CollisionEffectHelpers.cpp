#include "Sandbox/utilities/CollisionEffectHelpers.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interfaces/CollisionEffectComponent.h"
#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

void CollisionEffectHelpers::register_with_collision_system(UActorComponent* component) {
    if (!component) {
        return;
    }

    if (auto* world{get_world_from_component(component)}) {
        if (auto* subsystem{world->GetSubsystem<UCollisionEffectSubsystem>()}) {
            subsystem->register_effect_component(component);
        }
    }
}

void CollisionEffectHelpers::register_with_collision_system(AActor* actor) {
    if (!actor) {
        return;
    }

    if (auto* world{actor->GetWorld()}) {
        if (auto* subsystem{world->GetSubsystem<UCollisionEffectSubsystem>()}) {
            subsystem->register_actor(actor);
        }
    }
}

UWorld* CollisionEffectHelpers::get_world_from_component(UActorComponent* component) {
    if (component) {
        if (auto* owner{component->GetOwner()}) {
            return owner->GetWorld();
        }
    }
    return nullptr;
}

UWorld* CollisionEffectHelpers::get_world_from_actor(AActor* actor) {
    return actor ? actor->GetWorld() : nullptr;
}
