#pragma once

#include "LevelPresentationSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

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
    float soft_target_pulse_opacity_boost{0.08f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", Units = "s"))
    float soft_target_pulse_duration{0.15f};

    UPROPERTY(EditAnywhere,
              Category = "Entity Overlay|Soft Target",
              meta = (ClampMin = "0.0", Units = "s"))
    float soft_target_fade_out_duration{0.15f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target")
    TObjectPtr<UStaticMesh> soft_target_world_mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay|Soft Target")
    TObjectPtr<UMaterialInterface> soft_target_world_material{nullptr};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor background_color{0.02f, 0.02f, 0.02f, 0.85f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor fill_color{0.10f, 0.85f, 0.20f, 1.0f};
};
USTRUCT(BlueprintType)
struct FRadarSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Radar")
    bool enabled{true};

    UPROPERTY(EditAnywhere, Category = "Radar", meta = (ClampMin = "0.0", Units = "cm"))
    float combat_range{100000.0f};

    UPROPERTY(EditAnywhere, Category = "Radar", meta = (ClampMin = "0.0", Units = "cm"))
    float tactical_range{400000.0f};

    UPROPERTY(EditAnywhere, Category = "Radar", meta = (ClampMin = "0.0", Units = "cm"))
    float maximum_range{2000000.0f};

    UPROPERTY(EditAnywhere, Category = "Radar", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float combat_display_radius{0.45f};

    UPROPERTY(EditAnywhere,
              Category = "Radar",
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float grid_opacity{0.55f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Grid",
              meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
    float core_cell_radius{0.18f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Grid",
              meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
    float combat_cell_radius{0.075f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Grid",
              meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
    float tactical_cell_radius{0.045f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Grid",
              meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
    float strategic_cell_radius{0.025f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Glyphs",
              meta = (ClampMin = "0.5", ClampMax = "2.0", UIMin = "0.5", UIMax = "2.0"))
    float glyph_size_scale{1.0f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Glyphs|Objectives",
              meta = (ClampMin = "1.0", ClampMax = "2.0", UIMin = "1.0", UIMax = "2.0"))
    float objective_size_multiplier{1.2f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Glyphs|Objectives",
              meta = (ClampMin = "0.0", ClampMax = "12.0", UIMin = "0.0", UIMax = "12.0"))
    float objective_ring_padding_pixels{3.0f};

    UPROPERTY(EditAnywhere,
              Category = "Radar|Glyphs|Objectives",
              meta = (ClampMin = "0.5", ClampMax = "4.0", UIMin = "0.5", UIMax = "4.0"))
    float objective_ring_thickness_pixels{1.25f};
};
