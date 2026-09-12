#include "sandbox/core/graph_plot.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ml::graph {
namespace {
auto sample_x(SeriesView const series, std::size_t const index) noexcept -> double {
    return series.x.empty() ? static_cast<double>(index) : static_cast<double>(series.x[index]);
}

auto lower_bound_x(SeriesView const series, double const value) noexcept -> std::size_t {
    std::size_t first{};
    auto count{series.y.size()};
    while (count > 0) {
        auto const step{count / 2};
        auto const index{first + step};
        if (sample_x(series, index) < value) {
            first = index + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

auto upper_bound_x(SeriesView const series, double const value) noexcept -> std::size_t {
    std::size_t first{};
    auto count{series.y.size()};
    while (count > 0) {
        auto const step{count / 2};
        auto const index{first + step};
        if (sample_x(series, index) <= value) {
            first = index + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

auto expanded_auto_range(double const min, double const max) noexcept -> Range {
    if (!std::isfinite(min) || !std::isfinite(max)) {
        return {};
    }
    if (min < max) {
        return {min, max};
    }
    if (min == 0.0) {
        return {-1.0, 1.0};
    }

    auto const padding{std::max(std::abs(min) * 0.05,
                                std::numeric_limits<double>::epsilon() * std::abs(min) * 16.0)};
    return {min - padding, max + padding};
}
}

auto is_valid_fixed_range(AxisSettings const axis) noexcept -> bool {
    return axis.range_mode != RangeMode::Fixed ||
           (std::isfinite(axis.fixed_range.min) && std::isfinite(axis.fixed_range.max) &&
            axis.fixed_range.min < axis.fixed_range.max);
}

auto is_valid_series(SeriesView const series) noexcept -> bool {
    if (!series.x.empty() && series.x.size() != series.y.size()) {
        return false;
    }

    float previous_x{-std::numeric_limits<float>::infinity()};
    for (std::size_t i{0}; i < series.y.size(); ++i) {
        if (!std::isfinite(series.y[i])) {
            return false;
        }
        if (!series.x.empty()) {
            auto const x{series.x[i]};
            if (!std::isfinite(x) || x < previous_x) {
                return false;
            }
            previous_x = x;
        }
    }
    return true;
}

auto resolve_x_range(std::span<SeriesView const> const series,
                     std::span<std::uint8_t const> const valid_series,
                     AxisSettings const axis) noexcept -> Range {
    if (axis.range_mode == RangeMode::Fixed) {
        return axis.fixed_range;
    }

    auto min_x{std::numeric_limits<double>::infinity()};
    auto max_x{-std::numeric_limits<double>::infinity()};
    bool found_x{false};
    for (std::size_t i{}; i < series.size(); ++i) {
        if (valid_series[i] == 0 || series[i].y.empty()) {
            continue;
        }
        min_x = std::min(min_x, sample_x(series[i], 0));
        max_x = std::max(max_x, sample_x(series[i], series[i].y.size() - 1));
        found_x = true;
    }
    if (found_x && axis.range_mode == RangeMode::AutoIncludeZero) {
        min_x = std::min(min_x, 0.0);
        max_x = std::max(max_x, 0.0);
    }
    return expanded_auto_range(min_x, max_x);
}

auto resolve_y_range(std::span<SeriesView const> const series,
                     std::span<std::uint8_t const> const valid_series,
                     AxisSettings const axis,
                     Range const x_range) noexcept -> Range {
    if (axis.range_mode == RangeMode::Fixed) {
        return axis.fixed_range;
    }

    auto min_y{std::numeric_limits<double>::infinity()};
    auto max_y{-std::numeric_limits<double>::infinity()};
    bool found_y{false};
    for (std::size_t series_index{}; series_index < series.size(); ++series_index) {
        if (valid_series[series_index] == 0) {
            continue;
        }
        auto const source{series[series_index]};
        auto const begin{lower_bound_x(source, x_range.min)};
        auto const end{upper_bound_x(source, x_range.max)};
        for (auto i{begin}; i < end; ++i) {
            auto const y{static_cast<double>(source.y[i])};
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
            found_y = true;
        }
    }
    if (found_y && axis.range_mode == RangeMode::AutoIncludeZero) {
        min_y = std::min(min_y, 0.0);
        max_y = std::max(max_y, 0.0);
    }
    return expanded_auto_range(min_y, max_y);
}

auto build_data_points(SeriesView const series,
                       Range const x_range,
                       float const plot_width,
                       bool& decimated) -> std::vector<Point2d> {
    std::vector<Point2d> result;
    if (series.y.empty()) {
        return result;
    }

    auto const first_inside{lower_bound_x(series, x_range.min)};
    auto const after_inside{upper_bound_x(series, x_range.max)};
    auto const candidate_begin{first_inside > 0 ? first_inside - 1 : 0};
    auto const candidate_end{std::min(after_inside + 1, series.y.size())};
    auto const candidate_count{candidate_end - candidate_begin};
    auto const bucket_count{static_cast<std::size_t>(std::max(std::ceil(plot_width), 1.0f))};
    auto last_sample_index{series.y.size()};
    auto append_sample{[&](std::size_t const index) {
        if (index == last_sample_index || index >= series.y.size()) {
            return;
        }
        result.push_back({sample_x(series, index), static_cast<double>(series.y[index])});
        last_sample_index = index;
    }};

    if (candidate_count <= bucket_count * 2) {
        result.reserve(candidate_count);
        for (auto i{candidate_begin}; i < candidate_end; ++i) {
            append_sample(i);
        }
        return result;
    }

    decimated = true;
    result.reserve(bucket_count * 2 + 2);
    if (first_inside > 0) {
        append_sample(first_inside - 1);
    }

    auto const x_span{x_range.max - x_range.min};
    auto current_bucket{bucket_count};
    auto min_index{series.y.size()};
    auto max_index{series.y.size()};
    float min_y{};
    float max_y{};
    auto flush_bucket{[&] {
        if (min_index == series.y.size()) {
            return;
        }
        if (min_index <= max_index) {
            append_sample(min_index);
            append_sample(max_index);
        } else {
            append_sample(max_index);
            append_sample(min_index);
        }
    }};

    for (auto i{first_inside}; i < after_inside; ++i) {
        auto const normalized_x{(sample_x(series, i) - x_range.min) / x_span};
        auto const raw_bucket{static_cast<std::ptrdiff_t>(
            std::floor(normalized_x * static_cast<double>(bucket_count)))};
        auto const bucket{static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(
            raw_bucket, 0, static_cast<std::ptrdiff_t>(bucket_count - 1)))};
        if (bucket != current_bucket) {
            flush_bucket();
            current_bucket = bucket;
            min_index = i;
            max_index = i;
            min_y = series.y[i];
            max_y = series.y[i];
            continue;
        }
        if (series.y[i] < min_y) {
            min_y = series.y[i];
            min_index = i;
        }
        if (series.y[i] > max_y) {
            max_y = series.y[i];
            max_index = i;
        }
    }
    flush_bucket();

    if (after_inside < series.y.size()) {
        append_sample(after_inside);
    }
    return result;
}

auto transform_points(std::span<Point2d const> const points,
                      Range const x_range,
                      Range const y_range,
                      float const plot_width,
                      float const plot_height,
                      Interpolation const interpolation) -> std::vector<Point2f> {
    auto const point_count{points.size()};
    auto const render_count{interpolation == Interpolation::StepAfter && point_count > 1
                                ? point_count * 2 - 1
                                : point_count};
    std::vector<Point2f> result;
    result.reserve(render_count);

    auto const x_span{x_range.max - x_range.min};
    auto const y_span{y_range.max - y_range.min};
    auto const transform{[&](Point2d const point) {
        auto const x_alpha{(point.x - x_range.min) / x_span};
        auto const y_alpha{(point.y - y_range.min) / y_span};
        return Point2f{static_cast<float>(x_alpha * plot_width),
                       static_cast<float>((1.0 - y_alpha) * plot_height)};
    }};

    if (interpolation == Interpolation::StepAfter && point_count > 1) {
        result.push_back(transform(points[0]));
        for (std::size_t i{1}; i < point_count; ++i) {
            result.push_back(transform({points[i].x, points[i - 1].y}));
            result.push_back(transform(points[i]));
        }
        return result;
    }

    for (auto const point : points) {
        result.push_back(transform(point));
    }
    return result;
}
}
