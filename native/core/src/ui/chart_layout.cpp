#include "sandbox/core/ui/chart_layout.h"

#include <algorithm>

namespace ml::ui::chart_layout {
auto make_layout(Vector2f const widget_size, Settings const settings) noexcept -> Layout {
    auto const available_width{
        std::max(widget_size.x - settings.padding.left - settings.padding.right, 0.0f)};
    auto const available_height{
        std::max(widget_size.y - settings.padding.top - settings.padding.bottom, 0.0f)};
    auto const label_height{std::min(settings.label_area_height, available_height)};
    return {
        .plot_origin = {settings.padding.left + settings.axis_thickness, settings.padding.top},
        .plot_size = {std::max(available_width - settings.axis_thickness, 0.0f),
                      std::max(available_height - label_height - settings.axis_thickness, 0.0f)},
        .label_area_height = label_height};
}
}
