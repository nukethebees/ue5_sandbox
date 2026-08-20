#pragma once

#include "SandboxGameShared/utilities/ActorCorners.h"

#include <SandboxCoreEngine/actor_utils.h>

#include <CoreMinimal.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <Math/Box.h>
#include <Math/Vector.h>

#include <concepts>

class AActor;

namespace ml {

/**
 * Returns the best available display name for an actor.
 * In editor builds, returns GetActorLabel() for human-readable names.
 * In shipping builds, falls back to GetName() for internal object names.
 *
 * @param actor The actor to get the display name for (can be null)
 * @return Human-readable name in editor builds, object name in shipping builds, or "null" if actor
 * is null
 */
auto SANDBOXGAMESHARED_API get_best_display_name(AActor const& actor) -> FString;

auto SANDBOXGAMESHARED_API get_actor_corners(AActor const& actor) -> FActorCorners;
auto SANDBOXGAMESHARED_API get_static_meshes_bounding_box(AActor const& actor) -> FBox;

auto SANDBOXGAMESHARED_API actor_is_within(AActor const& actor,
                                           AActor const& other,
                                           bool only_colliding_components)
    -> bool;

void SANDBOXGAMESHARED_API face_actor(AActor& actor, AActor const& other);
void SANDBOXGAMESHARED_API face_point(AActor& actor, FVector const& point);

void SANDBOXGAMESHARED_API fatal_if_actor_transform_not_identity(AActor const& actor);
void SANDBOXGAMESHARED_API fatal_if_actor_root_not_static(AActor const& actor);

void SANDBOXGAMESHARED_API set_actor_component_mobility(AActor& actor,
                                                         EComponentMobility::Type mobility);

template <typename T>
void destroy_all_actors(T& actors) {
    for (auto* a : actors) {
        if (IsValid(a)) {
            a->Destroy();
        }
    }
}

template <typename T>
    requires std::derived_from<T, AActor>
auto ensure_actor_exists(UWorld& world, TSubclassOf<T> subclass) -> T* {
    if (auto* const actor{get_first_actor<T>(world)}) {
        return actor;
    }

    auto* const actor{world.SpawnActor<T>(subclass)};
    if (IsValid(actor)) {
        UE_LOG(LogTemp, Display, TEXT("Spawned missing %s"), *T::StaticClass()->GetName());
    }

    return actor;
}
} // namespace ml
