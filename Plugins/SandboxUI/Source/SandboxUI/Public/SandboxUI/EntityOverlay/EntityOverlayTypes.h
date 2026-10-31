#pragma once

#include <ioj/ui/entity_overlay.h>

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

struct SANDBOXUI_API FEntityOverlaySourceView {
    TConstArrayView<FVector3f> positions;
    TConstArrayView<float> health_values;
    TConstArrayView<float> world_radii;

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return ioj::ui::entity_overlay::source_sizes_match(
            positions.Num(), health_values.Num(), world_radii.Num());
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
    FLinearColor background_color{0.02f, 0.02f, 0.02f, 0.85f};
    FLinearColor fill_color{0.10f, 0.85f, 0.20f, 1.0f};
    FLinearColor defend_objective_color{0.85f, 0.60f, 0.08f, 1.0f};
    FLinearColor destroy_objective_color{0.75f, 0.15f, 0.08f, 1.0f};
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
    TArray<ioj::ui::entity_overlay::Instance> instances;
};

class FEntityOverlayCollector {
  public:
    FEntityOverlayCollector() = default;

    SANDBOXUI_API void begin(FVector3f origin,
                             float maximum_range,
                             TArray<ioj::ui::entity_overlay::Instance>& output_instances);
    [[nodiscard]] SANDBOXUI_API auto try_add(FVector3f position,
                                             float normalized_health,
                                             float world_radius,
                                             ioj::ui::entity_overlay::ObjectiveRole objective_role =
                                                 ioj::ui::entity_overlay::ObjectiveRole::None,
                                             bool bypass_range = false) -> bool;
    [[nodiscard]] SANDBOXUI_API auto
        try_add_colored(FVector3f position,
                        float normalized_health,
                        float world_radius,
                        FLinearColor fill_color,
                        ioj::ui::entity_overlay::ObjectiveRole objective_role =
                            ioj::ui::entity_overlay::ObjectiveRole::None,
                        bool bypass_range = false) -> bool;
    [[nodiscard]] SANDBOXUI_API auto append(FEntityOverlaySourceView source) -> int32;

    [[nodiscard]] auto invalid_health_count() const noexcept -> int32 {
        return collector_.invalid_health_count();
    }
  private:
    [[nodiscard]] auto try_add_impl(FVector3f position,
                                    float normalized_health,
                                    float world_radius,
                                    uint32 display_data,
                                    bool bypass_range) -> bool;

    ioj::ui::entity_overlay::Collector collector_;
    TArray<ioj::ui::entity_overlay::Instance>* output_instances_{nullptr};
};
