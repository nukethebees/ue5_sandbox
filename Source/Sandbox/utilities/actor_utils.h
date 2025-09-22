#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

namespace ActorUtils {

/**
 * Returns the best available display name for an actor.
 * In editor builds, returns GetActorLabel() for human-readable names.
 * In shipping builds, falls back to GetName() for internal object names.
 *
 * @param actor The actor to get the display name for (can be null)
 * @return Human-readable name in editor builds, object name in shipping builds, or "null" if actor
 * is null
 */
inline FString GetBestDisplayName(AActor const* actor) {
    if (!actor) {
        return TEXT("null");
    }

#if WITH_EDITOR
    return actor->GetActorLabel();
#else
    return actor->GetName();
#endif
}

} // namespace ActorUtils
