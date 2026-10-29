#include "ioj/ui/chart_layout.h"

#include <algorithm>
#include <cmath>

namespace ioj::ui::chart_layout {
auto is_valid(Vector2f const desired_size, Settings const settings) noexcept -> bool {
    auto const nonnegative{[](float const value) { return std::isfinite(value) && value >= 0.0f; }};
    return nonnegative(desired_size.x()) && nonnegative(desired_size.y()) &&
           nonnegative(settings.padding.left) && nonnegative(settings.padding.right) &&
           nonnegative(settings.padding.top) && nonnegative(settings.padding.bottom) &&
           std::isfinite(settings.axis_thickness) && settings.axis_thickness > 0.0f &&
           nonnegative(settings.label_area_height) && nonnegative(settings.y_label_area_width);
}

auto make_layout(Vector2f const widget_size, Settings const settings) noexcept -> Layout {
    auto const available_width{
        std::max(widget_size.x() - settings.padding.left - settings.padding.right, 0.0f)};
    auto const available_height{
        std::max(widget_size.y() - settings.padding.top - settings.padding.bottom, 0.0f)};
    Layout result;
    if (!settings.show_axes) {
        result.plot_origin = {settings.padding.left, settings.padding.top};
        result.plot_size = {available_width, available_height};
        return result;
    }
    result.label_area_height = std::min(settings.label_area_height, available_height);
    result.y_label_area_width = std::min(settings.y_label_area_width, available_width);
    result.plot_origin = {settings.padding.left + result.y_label_area_width +
                              settings.axis_thickness,
                          settings.padding.top};
    result.plot_size = {
        std::max(available_width - result.y_label_area_width - settings.axis_thickness, 0.0f),
        std::max(available_height - result.label_area_height - settings.axis_thickness, 0.0f)};
    return result;
}
}
