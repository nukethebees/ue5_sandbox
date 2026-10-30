#pragma once

#include "ioj/ui/types.h"
#include <ioj/constrained_numbers.h>

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

class Settings {
    using Value = ConstrainedValue<Options, settings_policy>;
  public:
    constexpr Settings() = default;
    explicit consteval Settings(Options const options)
        : value_{options} {}

    static constexpr auto try_make(Options const options) -> std::optional<Settings> {
        auto const value{Value::try_make(options)};
        return value ? std::optional{Settings{*value}} : std::nullopt;
    }

    auto get_ref() const& noexcept -> Options const& { return value_.get_ref(); }
    auto get_ref() const&& -> Options const& = delete;
  private:
    explicit constexpr Settings(Value const value)
        : value_{value} {}

    Value value_;
};

struct Layout {
    Vector2f plot_origin{};
    Vector2f plot_size{};
    float label_area_height{};
    float y_label_area_width{};
};

[[nodiscard]] auto make_layout(Vector2f widget_size, Settings settings) noexcept -> Layout;
}
