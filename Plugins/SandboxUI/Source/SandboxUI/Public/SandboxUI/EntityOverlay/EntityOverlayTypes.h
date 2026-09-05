#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Templates/SharedPointer.h"

struct SANDBOXUI_API FEntityOverlayInstance {
    FVector3f world_position{FVector3f::ZeroVector};
    float health{0.0f};
    float world_radius{0.0f};
};

static_assert(sizeof(FEntityOverlayInstance) == sizeof(float) * 5);

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
    FLinearColor background_color{0.02f, 0.02f, 0.02f, 0.85f};
    FLinearColor fill_color{0.10f, 0.85f, 0.20f, 1.0f};
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
};

class FEntityOverlayCollector {
  public:
    FEntityOverlayCollector() = default;

    SANDBOXUI_API void begin(FVector3f origin,
                             float maximum_range,
                             TArray<FEntityOverlayInstance>& output_instances);
    [[nodiscard]] SANDBOXUI_API auto
        try_add(FVector3f position, float normalized_health, float world_radius) -> bool;
    [[nodiscard]] SANDBOXUI_API auto append(FEntityOverlaySourceView source) -> int32;

    [[nodiscard]] auto invalid_health_count() const noexcept -> int32 {
        return invalid_health_count_;
    }
  private:
    FVector3f origin_{FVector3f::ZeroVector};
    float maximum_range_squared_{0.0f};
    TArray<FEntityOverlayInstance>* output_instances_{nullptr};
    int32 invalid_health_count_{0};
};
