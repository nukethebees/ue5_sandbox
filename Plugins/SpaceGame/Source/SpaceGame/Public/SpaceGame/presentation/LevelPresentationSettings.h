#pragma once

#include <SpaceGame/support/DrawDebugConfig.h>

#include "LevelPresentationSettings.generated.h"

USTRUCT()
struct FLevelPresentationSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig laser_debug_drawer;
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool laser_debug_shapes{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool capital_debug_shapes{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool fighter_debug_targets{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool fighter_debug_locations{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool turret_debug_targets{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool turret_debug_entities{false};
};
