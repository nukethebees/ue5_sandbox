#pragma once

#include <ioj/ui/chart_layout.h>

#include "Layout/Margin.h"
#include "Math/Vector2D.h"

namespace SandboxUI::Widgets {
inline auto is_finite_vector(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y);
}

inline auto chart_layout_settings(FMargin const padding,
                                  float const axis_thickness,
                                  float const label_height,
                                  float const label_width = 0.0f,
                                  bool const show_axes = true) -> ioj::ui::chart_layout::Settings {
    return {.padding = {padding.Left, padding.Top, padding.Right, padding.Bottom},
            .axis_thickness = axis_thickness,
            .label_area_height = label_height,
            .y_label_area_width = label_width,
            .show_axes = show_axes};
}
}
