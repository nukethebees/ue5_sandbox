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

auto is_valid_layout(LayoutSettings const settings) noexcept -> bool {
    return std::isfinite(settings.desired_width) && std::isfinite(settings.desired_height) &&
           settings.desired_width >= 0.0f && settings.desired_height >= 0.0f &&
           std::isfinite(settings.left_margin) && settings.left_margin >= 0.0f &&
           std::isfinite(settings.right_margin) && settings.right_margin >= 0.0f &&
           std::isfinite(settings.top_margin) && settings.top_margin >= 0.0f &&
           std::isfinite(settings.bottom_margin) && settings.bottom_margin >= 0.0f &&
           settings.target_x_ticks >= 0 && settings.target_y_ticks >= 0;
}

auto make_plot_layout(Point2f const extent, LayoutSettings const settings) noexcept -> PlotLayout {
    return {.origin = {settings.left_margin, settings.top_margin},
            .size = {std::max(0.0f, extent.x - settings.left_margin - settings.right_margin),
                     std::max(0.0f, extent.y - settings.top_margin - settings.bottom_margin)}};
}

auto nearest_sample_index(SeriesView const series, double const x) noexcept
    -> std::optional<std::size_t> {
    if (!series.x.empty() && series.x.size() != series.y.size()) {
        return std::nullopt;
    }

    std::optional<std::size_t> result;
    auto best_distance{std::numeric_limits<double>::max()};
    for (std::size_t index{}; index < series.y.size(); ++index) {
        auto const candidate{sample_x(series, index)};
        if (!std::isfinite(candidate)) {
            continue;
        }
        auto const distance{std::abs(candidate - x)};
        if (distance < best_distance) {
            best_distance = distance;
            result = index;
        }
    }
    return result;
}

auto nearest_x(std::span<SeriesView const> const series, double const x) noexcept
    -> std::optional<double> {
    std::optional<double> result;
    auto best_distance{std::numeric_limits<double>::max()};
    for (auto const& item : series) {
        auto const nearest{nearest_sample_index(item, x)};
        if (!nearest) {
            continue;
        }
        auto const candidate{sample_x(item, *nearest)};
        auto const distance{std::abs(candidate - x)};
        if (distance < best_distance) {
            best_distance = distance;
            result = candidate;
        }
    }
    return result;
}

auto build_ticks(Range const range,
                 float const extent,
                 std::int32_t const target_count,
                 bool const invert) -> std::vector<Tick> {
    if (extent <= 0.0f || target_count <= 0 || range.max <= range.min) {
        return {};
    }

    auto const raw_step{(range.max - range.min) / std::max(1, target_count)};
    auto const exponent{std::floor(std::log10(raw_step))};
    auto const magnitude{std::pow(10.0, exponent)};
    auto const normalized{raw_step / magnitude};
    auto const step_multiplier{normalized <= 1.0   ? 1.0
                               : normalized <= 2.0 ? 2.0
                               : normalized <= 5.0 ? 5.0
                                                   : 10.0};
    auto const step{step_multiplier * magnitude};
    auto const first{std::ceil(range.min / step) * step};
    auto const span{range.max - range.min};

    std::vector<Tick> result;
    result.reserve(static_cast<std::size_t>(target_count + 2));
    for (std::int32_t index{}; index < 64; ++index) {
        auto value{first + static_cast<double>(index) * step};
        if (value > range.max + step * 1e-6) {
            break;
        }
        if (std::abs(value) < step * 1e-9) {
            value = 0.0;
        }

        auto alpha{static_cast<float>((value - range.min) / span)};
        if (invert) {
            alpha = 1.0f - alpha;
        }
        result.push_back({value, alpha * extent});
    }
    return result;
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
