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

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Size Scaling", meta = (ClampMin = "0.01"))
    float minimum_bar_scale{0.5f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Size Scaling", meta = (ClampMin = "0.01"))
    float maximum_bar_scale{2.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay", meta = (ClampMin = "0.0"))
    float inset_pixels{1.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay", meta = (ClampMin = "0.0"))
    float maximum_inset_height_ratio{0.4f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Objectives", meta = (ClampMin = "0.0"))
    float objective_frame_pixels{3.0f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Objectives",
              meta = (ClampMin = "1.0", ClampMax = "2.0"))
    float objective_bar_height_scale{1.4f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Objectives", meta = (ClampMin = "0.0"))
    float screen_edge_padding_pixels{12.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "1.0"))
    float soft_target_acquisition_radius_pixels{72.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "1.0"))
    float soft_target_retention_radius_pixels{96.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "0.0"))
    float soft_target_centre_tie_radius_pixels{4.0f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float soft_target_switch_improvement_ratio{0.75f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "1.0"))
    float soft_target_approach_range_multiplier{4.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "1.0"))
    float soft_target_minimum_radius_pixels{28.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "1.0"))
    float soft_target_maximum_radius_pixels{96.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "0.0"))
    float soft_target_bounds_padding_pixels{10.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target", meta = (ClampMin = "1.0"))
    float soft_target_bracket_start_radius_multiplier{2.5f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float soft_target_opacity{0.70f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float soft_target_glow_opacity{0.045f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float soft_target_pulse_opacity_boost{0.08f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", Units = "s"))
    float soft_target_pulse_duration{0.15f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", Units = "s"))
    float soft_target_fade_out_duration{0.15f};

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
