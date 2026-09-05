#pragma once

#include <SpaceGame/support/DrawDebugConfig.h>

#include "LevelPresentationSettings.generated.h"

USTRUCT(BlueprintType)
struct FEntityOverlaySettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    bool enabled{true};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay", meta = (ClampMin = "0.0", Units = "cm"))
    float maximum_range{1000000.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay", meta = (ClampMin = "1.0"))
    FVector2D bar_size_pixels{64.0, 8.0};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FVector2D screen_offset_pixels{0.0, -24.0};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay", meta = (ClampMin = "0.0"))
    float inset_pixels{1.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor background_color{0.02f, 0.02f, 0.02f, 0.85f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor fill_color{0.10f, 0.85f, 0.20f, 1.0f};
};

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

    UPROPERTY(EditAnywhere, Category = "UI", meta = (ShowOnlyInnerProperties))
    FEntityOverlaySettings entity_overlay;
};
