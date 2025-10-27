#pragma once

#include <array>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Vector.h"

#include "Sandbox/environment/data/ActorCorners.h"

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
inline FString get_best_display_name(AActor const& actor) {
#if WITH_EDITOR
    return actor.GetActorLabel();
#else
    return actor.GetName();
#endif
}

auto get_actor_corners(AActor const& actor) -> FActorCorners;

} // namespace ml
