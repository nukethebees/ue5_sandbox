#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Templates/SharedPointer.h"

#include <cstddef>
#include <type_traits>

enum class EEntityOverlayObjectiveRole : uint32 {
    None,
    Defend,
    Destroy,
};

enum class EEntityOverlaySoftTargetRole : uint32 {
    None,
    Active,
    Fading,
};

struct SANDBOXUI_API FEntityOverlayInstance {
    static constexpr uint32 soft_target_role_shift{3};
    static constexpr uint32 soft_target_role_mask{0x3u << soft_target_role_shift};

    FVector3f world_position{FVector3f::ZeroVector};
    float health{0.0f};
    float world_radius{0.0f};
    uint32 display_data{0};

    [[nodiscard]] auto is_soft_target() const noexcept -> bool {
        return (display_data & soft_target_role_mask) != 0;
    }

    [[nodiscard]] auto soft_target_role() const noexcept -> EEntityOverlaySoftTargetRole {
        return static_cast<EEntityOverlaySoftTargetRole>((display_data & soft_target_role_mask) >>
                                                         soft_target_role_shift);
    }
};

static_assert(sizeof(FEntityOverlayInstance) == sizeof(float) * 6);
static_assert(std::is_standard_layout_v<FEntityOverlayInstance>);
static_assert(std::is_trivially_copyable_v<FEntityOverlayInstance>);
static_assert(offsetof(FEntityOverlayInstance, world_position) == 0);
static_assert(offsetof(FEntityOverlayInstance, health) == sizeof(float) * 3);
static_assert(offsetof(FEntityOverlayInstance, world_radius) == sizeof(float) * 4);
static_assert(offsetof(FEntityOverlayInstance, display_data) == sizeof(float) * 5);

struct SANDBOXUI_API FEntityOverlaySourceView {
    TConstArrayView<FVector3f> positions;
    TConstArrayView<float> health_values;
    TConstArrayView<float> world_radii;

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return positions.Num() == health_values.Num() && positions.Num() == world_radii.Num();
    }
};

struct SANDBOXUI_API FEntityOverlayStyle {
    FVector2f bar_size_pixels{64.0f, 8.0f};
    FVector2f screen_offset_pixels{0.0f, -24.0f};
    float minimum_world_radius{100.0f};
    float maximum_world_radius{1000.0f};
    float minimum_bar_scale{0.5f};
    float maximum_bar_scale{2.0f};
    float inset_pixels{1.0f};
    float maximum_inset_height_ratio{0.4f};
    float objective_bar_height_scale{1.4f};
    float objective_frame_pixels{3.0f};
    float screen_edge_padding_pixels{12.0f};
    float soft_target_bracket_start_radius_multiplier{2.5f};
    float soft_target_opacity{0.70f};
    float soft_target_glow_opacity{0.045f};
    float soft_target_pulse_opacity_boost{0.08f};
    FLinearColor background_color{0.02f, 0.02f, 0.02f, 0.85f};
    FLinearColor fill_color{0.10f, 0.85f, 0.20f, 1.0f};
    FLinearColor defend_objective_color{0.85f, 0.60f, 0.08f, 1.0f};
    FLinearColor destroy_objective_color{0.75f, 0.15f, 0.08f, 1.0f};
    FLinearColor soft_target_neutral_color{0.72f, 0.70f, 0.65f, 1.0f};
    FLinearColor soft_target_in_range_color{0.84f, 0.65f, 0.23f, 1.0f};
};

struct SANDBOXUI_API FEntityOverlayView {
    FVector3f camera_origin{FVector3f::ZeroVector};
    FMatrix44f view_projection{FMatrix44f::Identity};
    FIntRect view_rect{};
    FIntPoint output_size{};

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return output_size.X > 0 && output_size.Y > 0 && view_rect.Width() > 0 &&
               view_rect.Height() > 0;
    }
};

struct SANDBOXUI_API FEntityOverlayFrame {
    TArray<FEntityOverlayInstance> instances;
    float soft_target_range_progress{0.0f};
    float soft_target_radius_pixels{0.0f};
    float soft_target_pulse{0.0f};
    float soft_target_visibility{1.0f};
    bool soft_target_in_range{false};
    float fading_soft_target_range_progress{0.0f};
    float fading_soft_target_radius_pixels{0.0f};
    float fading_soft_target_visibility{0.0f};
    bool fading_soft_target_in_range{false};
};

class FEntityOverlayCollector {
  public:
    FEntityOverlayCollector() = default;

    SANDBOXUI_API void begin(FVector3f origin,
                             float maximum_range,
                             TArray<FEntityOverlayInstance>& output_instances);
    [[nodiscard]] SANDBOXUI_API auto
        try_add(FVector3f position,
                float normalized_health,
                float world_radius,
                EEntityOverlayObjectiveRole objective_role = EEntityOverlayObjectiveRole::None,
                bool bypass_range = false,
                EEntityOverlaySoftTargetRole soft_target_role = EEntityOverlaySoftTargetRole::None)
            -> bool;
    [[nodiscard]] SANDBOXUI_API auto try_add_colored(
        FVector3f position,
        float normalized_health,
        float world_radius,
        FLinearColor fill_color,
        EEntityOverlayObjectiveRole objective_role = EEntityOverlayObjectiveRole::None,
        bool bypass_range = false,
        EEntityOverlaySoftTargetRole soft_target_role = EEntityOverlaySoftTargetRole::None) -> bool;
    [[nodiscard]] SANDBOXUI_API auto append(FEntityOverlaySourceView source) -> int32;

    [[nodiscard]] auto invalid_health_count() const noexcept -> int32 {
        return invalid_health_count_;
    }
  private:
    [[nodiscard]] auto try_add_impl(FVector3f position,
                                    float normalized_health,
                                    float world_radius,
                                    uint32 display_data,
                                    bool bypass_range) -> bool;

    FVector3f origin_{FVector3f::ZeroVector};
    float maximum_range_squared_{0.0f};
    TArray<FEntityOverlayInstance>* output_instances_{nullptr};
    int32 first_objective_index_{INDEX_NONE};
    int32 invalid_health_count_{0};
};
