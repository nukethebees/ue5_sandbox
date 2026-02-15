#pragma once

#include "CoreMinimal.h"

#include "Sandbox/environment/data/ActorCorners.h"

#include "MyPlayerControllerCache.generated.h"

USTRUCT(BlueprintType)
struct FMyPlayerControllerCache {
    GENERATED_BODY()

    FMyPlayerControllerCache() = default;

    FVector camera_location;
    FRotator camera_rotation;
    FActorCorners outline_corners;
};
