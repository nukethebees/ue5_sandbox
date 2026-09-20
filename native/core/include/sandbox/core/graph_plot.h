#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ml::graph {
enum class RangeMode : std::uint8_t {
    Auto,
    AutoIncludeZero,
    Fixed,
};

enum class Interpolation : std::uint8_t {
    Linear,
    StepAfter,
};

struct Range {
    double min{0.0};
    double max{1.0};

    auto operator==(Range const&) const -> bool = default;
};

struct AxisSettings {
    RangeMode range_mode{RangeMode::Auto};
    Range fixed_range{};
};

struct SeriesView {
    std::span<float const> x;
    std::span<float const> y;
};

struct Point2d {
    double x{};
    double y{};

    auto operator==(Point2d const&) const -> bool = default;
};

struct Point2f {
    float x{};
    float y{};
};

struct LayoutSettings {
    float desired_width{320.0f};
    float desired_height{180.0f};
    float left_margin{54.0f};
    float right_margin{12.0f};
    float top_margin{12.0f};
    float bottom_margin{26.0f};
    std::int32_t target_x_ticks{6};
    std::int32_t target_y_ticks{5};
};

struct PlotLayout {
    Point2f origin{};
    Point2f size{};
};

struct Tick {
    double value{};
    float position{};
};

[[nodiscard]] auto is_valid_fixed_range(AxisSettings axis) noexcept -> bool;
[[nodiscard]] auto is_valid_series(SeriesView series) noexcept -> bool;
[[nodiscard]] auto is_valid_layout(LayoutSettings settings) noexcept -> bool;
[[nodiscard]] auto make_plot_layout(Point2f extent, LayoutSettings settings) noexcept -> PlotLayout;
[[nodiscard]] auto nearest_sample_index(SeriesView series, double x) noexcept
    -> std::optional<std::size_t>;
[[nodiscard]] auto nearest_x(std::span<SeriesView const> series, double x) noexcept
    -> std::optional<double>;
[[nodiscard]] auto build_ticks(Range range, float extent, std::int32_t target_count, bool invert)
    -> std::vector<Tick>;
[[nodiscard]] auto resolve_x_range(std::span<SeriesView const> series,
                                   std::span<std::uint8_t const> valid_series,
                                   AxisSettings axis) noexcept -> Range;
[[nodiscard]] auto resolve_y_range(std::span<SeriesView const> series,
                                   std::span<std::uint8_t const> valid_series,
                                   AxisSettings axis,
                                   Range x_range) noexcept -> Range;
[[nodiscard]] auto
    build_data_points(SeriesView series, Range x_range, float plot_width, bool& decimated)
        -> std::vector<Point2d>;
[[nodiscard]] auto transform_points(std::span<Point2d const> points,
                                    Range x_range,
                                    Range y_range,
                                    float plot_width,
                                    float plot_height,
                                    Interpolation interpolation) -> std::vector<Point2f>;
}
