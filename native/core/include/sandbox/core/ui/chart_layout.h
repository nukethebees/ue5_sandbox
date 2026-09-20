#pragma once

#include "sandbox/core/ui/types.h"

namespace ml::ui::chart_layout {
struct Settings {
    Insets padding{};
    float axis_thickness{1.0f};
    float label_area_height{20.0f};
};

struct Layout {
    Vector2f plot_origin{};
    Vector2f plot_size{};
    float label_area_height{};
};

[[nodiscard]] auto make_layout(Vector2f widget_size, Settings settings) noexcept -> Layout;
}
