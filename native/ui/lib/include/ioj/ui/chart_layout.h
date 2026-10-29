#pragma once

#include "ioj/ui/types.h"

namespace ioj::ui::chart_layout {
struct Settings {
    Insets padding{};
    float axis_thickness{1.0f};
    float label_area_height{20.0f};
    float y_label_area_width{};
    bool show_axes{true};
};

struct Layout {
    Vector2f plot_origin{};
    Vector2f plot_size{};
    float label_area_height{};
    float y_label_area_width{};
};

[[nodiscard]] auto is_valid(Vector2f desired_size, Settings settings) noexcept -> bool;
[[nodiscard]] auto make_layout(Vector2f widget_size, Settings settings) noexcept -> Layout;
}
