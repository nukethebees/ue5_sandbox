#pragma once

#include "LevelPresentationSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FSoftTargetSettings {
    GENERATED_BODY()

    float acquisition_radius_pixels{72.0f};
    float retention_radius_pixels{96.0f};
    float centre_tie_radius_pixels{4.0f};
    float switch_improvement_ratio{0.75f};
    float range_transition_start_multiplier{4.0f};
    float minimum_radius_pixels{28.0f};
    float maximum_radius_pixels{96.0f};
    float bounds_padding_pixels{10.0f};
    float bracket_start_radius_multiplier{2.5f};
    float opacity{0.70f};
    float pulse_opacity_boost{0.08f};
    float pulse_duration{0.15f};
    float fade_out_duration{0.15f};

    UPROPERTY(EditAnywhere, Category = "Rendering")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Rendering")
    TObjectPtr<UMaterialInterface> material{nullptr};
};

USTRUCT(BlueprintType)
struct FEntityOverlaySettings {
    GENERATED_BODY()

    bool enabled{true};
    float maximum_range{1000000.0f};
    FVector2D bar_size_pixels{64.0, 8.0};
    FVector2D screen_offset_pixels{0.0, -24.0};
    float minimum_bar_scale{0.5f};
    float maximum_bar_scale{2.0f};
    float inset_pixels{1.0f};
    float maximum_inset_height_ratio{0.4f};
    float objective_frame_pixels{3.0f};
    float objective_bar_height_scale{1.4f};
    float screen_edge_padding_pixels{12.0f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FSoftTargetSettings soft_target;

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor background_color{0.02f, 0.02f, 0.02f, 0.85f};

    UPROPERTY(EditAnywhere, Category = "Entity Overlay")
    FLinearColor fill_color{0.10f, 0.85f, 0.20f, 1.0f};
};

struct FRadarSettings {
    bool enabled{true};
    float combat_range{100000.0f};
    float tactical_range{400000.0f};
    float maximum_range{2000000.0f};
    float combat_display_radius{0.45f};
    float grid_opacity{0.55f};
    float core_cell_radius{0.18f};
    float combat_cell_radius{0.075f};
    float tactical_cell_radius{0.045f};
    float strategic_cell_radius{0.025f};
    float glyph_size_scale{1.0f};
    float objective_size_multiplier{1.2f};
    float objective_ring_padding_pixels{3.0f};
    float objective_ring_thickness_pixels{1.25f};
};
