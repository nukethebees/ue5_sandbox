#pragma once

#include "ioj/ui/types.h"
#include <ioj/constrained_numbers.h>

#include <algorithm>

namespace ioj::ui::chart_layout {
struct Options {
    Insets padding{};
    float axis_thickness{1.0f};
    float label_area_height{20.0f};
    float y_label_area_width{};
    bool show_axes{true};
};

inline constexpr auto settings_policy{ConstrainedValuePolicy{
    [](Options const& value) {
        return NonNegativeFinite<float>::valid(value.padding.left) &&
               NonNegativeFinite<float>::valid(value.padding.top) &&
               NonNegativeFinite<float>::valid(value.padding.right) &&
               NonNegativeFinite<float>::valid(value.padding.bottom) &&
               PositiveFinite<float>::valid(value.axis_thickness) &&
               NonNegativeFinite<float>::valid(value.label_area_height) &&
               NonNegativeFinite<float>::valid(value.y_label_area_width);
    },
    "Chart layout requires finite nonnegative dimensions and positive axis thickness"}};

using Settings = ConstrainedValue<Options, settings_policy>;

struct Layout {
    Vector2f plot_origin{};
    Vector2f plot_size{};
    float label_area_height{};
    float y_label_area_width{};
};

[[nodiscard]] inline auto make_layout(Vector2f const widget_size,
                                      Settings const validated_settings) noexcept -> Layout {
    auto const& settings{validated_settings.get_ref()};
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
