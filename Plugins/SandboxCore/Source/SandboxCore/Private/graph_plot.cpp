#include "SandboxCore/graph_plot.h"

#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"

#include <sandbox/core/graph_plot.h>

#include <span>
#include <vector>

namespace {
auto to_native_axis(FGraphAxisSettings const axis) -> ml::graph::AxisSettings {
    auto const range_mode{[&] {
        switch (axis.range_mode) {
            using enum EGraphRangeMode;
            case Auto:
                return ml::graph::RangeMode::Auto;
            case AutoIncludeZero:
                return ml::graph::RangeMode::AutoIncludeZero;
            case Fixed:
                return ml::graph::RangeMode::Fixed;
        }
        return ml::graph::RangeMode::Auto;
    }()};
    return {.range_mode = range_mode,
            .fixed_range = {.min = axis.fixed_range.min, .max = axis.fixed_range.max}};
}

auto to_native_series(FGraphSeriesView const& series) -> ml::graph::SeriesView {
    return {
        .x = {series.x.GetData(), static_cast<std::size_t>(series.x.Num())},
        .y = {series.y.GetData(), static_cast<std::size_t>(series.y.Num())},
    };
}

auto to_native_interpolation(EGraphSeriesInterpolation const interpolation)
    -> ml::graph::Interpolation {
    return interpolation == EGraphSeriesInterpolation::StepAfter
             ? ml::graph::Interpolation::StepAfter
             : ml::graph::Interpolation::Linear;
}

auto to_unreal_range(ml::graph::Range const range) -> FGraphRange {
    return {.min = range.min, .max = range.max};
}

bool text_equal(FText const& lhs, FText const& rhs) {
    return lhs.IdenticalTo(rhs, ETextIdenticalModeFlags::None);
}

template <typename T>
bool views_equal(TConstArrayView<T> const lhs, TConstArrayView<T> const rhs) {
    return lhs.GetData() == rhs.GetData() && lhs.Num() == rhs.Num();
}

bool descriptors_equal(FGraphSeriesView const& lhs, FGraphSeriesView const& rhs) {
    return views_equal(lhs.x, rhs.x) && views_equal(lhs.y, rhs.y) && lhs.style == rhs.style &&
           text_equal(lhs.name, rhs.name);
}
}

bool FGraphRenderCache::set_series(TConstArrayView<FGraphSeriesView> const series,
                                   uint64 const data_revision) {
    if (has_data_revision_ && data_revision < data_revision_) {
        ensureMsgf(false,
                   TEXT("Graph data revision must be monotonically increasing (%llu < %llu)."),
                   data_revision,
                   data_revision_);
        return false;
    }

    auto const revision_changed{!has_data_revision_ || data_revision != data_revision_};
    bool descriptors_changed{false};
    if (!revision_changed) {
        descriptors_changed = series_.Num() != series.Num();
        auto const count{series.Num()};
        for (int32 i{0}; i < count && !descriptors_changed; ++i) {
            descriptors_changed = !descriptors_equal(series_[i], series[i]);
        }
    }

    if (!descriptors_changed && !revision_changed) {
        return false;
    }

    series_.Reset(series.Num());
    series_.Append(series.GetData(), series.Num());
    cached_series_.SetNum(series.Num());
    data_revision_ = data_revision;
    has_data_revision_ = true;
    dirty_ = true;
    return true;
}

bool FGraphRenderCache::set_axis_settings(FGraphAxisSettings const x_axis,
                                          FGraphAxisSettings const y_axis) {
    if (!ml::graph::is_valid_fixed_range(to_native_axis(x_axis)) ||
        !ml::graph::is_valid_fixed_range(to_native_axis(y_axis))) {
        ensureMsgf(false, TEXT("Fixed graph ranges must be finite and have min < max."));
        return false;
    }
    if (x_axis_ == x_axis && y_axis_ == y_axis) {
        return false;
    }

    x_axis_ = x_axis;
    y_axis_ = y_axis;
    dirty_ = true;
    return true;
}

bool FGraphRenderCache::update(FVector2f const plot_size) {
    auto const clamped_size{
        FVector2f{FMath::Max(plot_size.X, 0.0f), FMath::Max(plot_size.Y, 0.0f)}};
    if (plot_size_ != clamped_size) {
        plot_size_ = clamped_size;
        dirty_ = true;
    }
    if (!dirty_) {
        return false;
    }

    valid_series_.SetNumZeroed(series_.Num());
    stats_.source_sample_count = 0;
    stats_.emitted_point_count = 0;
    stats_.decimated = false;

    auto const series_count{series_.Num()};
    for (int32 i{0}; i < series_count; ++i) {
        valid_series_[i] = validate_series(series_[i], i) ? 1 : 0;
        if (valid_series_[i] != 0) {
            stats_.source_sample_count += series_[i].y.Num();
        }
    }

    resolve_ranges(valid_series_);

    for (int32 i{0}; i < series_count; ++i) {
        auto& cached{cached_series_[i]};
        cached.name = series_[i].name;
        cached.style = series_[i].style;
        if (valid_series_[i] == 0 || plot_size_.X <= 0.0f || plot_size_.Y <= 0.0f) {
            cached.data_points.Reset();
            cached.render_points.Reset();
            continue;
        }
        build_series(i, plot_size_);
        stats_.emitted_point_count += cached.render_points.Num();
    }

    ++stats_.rebuild_count;
    dirty_ = false;
    return true;
}

bool FGraphRenderCache::validate_series(FGraphSeriesView const& series,
                                        int32 const series_index) const {
    if (ml::graph::is_valid_series(to_native_series(series))) {
        return true;
    }

    ensureMsgf(false, TEXT("Graph series %d contains invalid X or Y data."), series_index);
    return false;
}

void FGraphRenderCache::resolve_ranges(TConstArrayView<uint8> const valid_series) {
    std::vector<ml::graph::SeriesView> native_series;
    native_series.reserve(static_cast<std::size_t>(series_.Num()));
    for (auto const& series : series_) {
        native_series.push_back(to_native_series(series));
    }

    auto const validity{
        std::span{valid_series.GetData(), static_cast<std::size_t>(valid_series.Num())}};
    auto const x_range{
        ml::graph::resolve_x_range(native_series, validity, to_native_axis(x_axis_))};
    auto const y_range{
        ml::graph::resolve_y_range(native_series, validity, to_native_axis(y_axis_), x_range)};
    x_range_ = to_unreal_range(x_range);
    y_range_ = to_unreal_range(y_range);
}

void FGraphRenderCache::build_series(int32 const series_index, FVector2f const plot_size) {
    auto const& source{series_[series_index]};
    auto& cached{cached_series_[series_index]};
    auto const x_range{ml::graph::Range{x_range_.min, x_range_.max}};
    auto const y_range{ml::graph::Range{y_range_.min, y_range_.max}};

    bool decimated{false};
    auto const data_points{
        ml::graph::build_data_points(to_native_series(source), x_range, plot_size.X, decimated)};
    auto const render_points{
        ml::graph::transform_points(data_points,
                                    x_range,
                                    y_range,
                                    plot_size.X,
                                    plot_size.Y,
                                    to_native_interpolation(source.style.interpolation))};

    cached.data_points.Reset(static_cast<int32>(data_points.size()));
    for (auto const point : data_points) {
        cached.data_points.Emplace(point.x, point.y);
    }
    cached.render_points.Reset(static_cast<int32>(render_points.size()));
    for (auto const point : render_points) {
        cached.render_points.Emplace(point.x, point.y);
    }
    stats_.decimated |= decimated;
}
