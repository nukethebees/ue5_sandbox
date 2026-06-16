#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Box.h"
#include "Math/Vector.h"

#include "Sandbox/utilities/ActorCorners.h"

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
auto SANDBOX_API get_best_display_name(AActor const& actor) -> FString;

auto get_actor_corners(AActor const& actor) -> FActorCorners;
auto get_static_meshes_bounding_box(AActor const& actor) -> FBox;

auto actor_is_within(AActor const& actor, AActor const& other, bool only_colliding_components)
    -> bool;

void face_actor(AActor& actor, AActor const& other);
void face_point(AActor& actor, FVector const& point);

void fatal_if_actor_transform_not_identity(AActor const& actor);
void fatal_if_actor_root_not_static(AActor const& actor);

void set_actor_component_mobility(AActor& actor, EComponentMobility::Type mobility);

template <typename T>
void destroy_all_actors(T& actors) {
    for (auto* a : actors) {
        if (IsValid(a)) { a->Destroy(); }
    }
}
} // namespace ml
