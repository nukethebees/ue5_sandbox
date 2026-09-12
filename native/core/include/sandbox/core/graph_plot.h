#pragma once

#include <cstdint>
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

[[nodiscard]] auto is_valid_fixed_range(AxisSettings axis) noexcept -> bool;
[[nodiscard]] auto is_valid_series(SeriesView series) noexcept -> bool;
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
