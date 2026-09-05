#include "SandboxCore/graph_plot.h"

#include "HAL/PlatformMath.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"

#include <limits>

namespace {
auto lower_bound_x(FGraphSeriesView const& series, double const value) -> int32 {
    int32 first{0};
    int32 count{series.y.Num()};
    while (count > 0) {
        auto const step{count / 2};
        auto const index{first + step};
        auto const x{series.x.IsEmpty() ? static_cast<double>(index)
                                        : static_cast<double>(series.x[index])};
        if (x < value) {
            first = index + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

auto upper_bound_x(FGraphSeriesView const& series, double const value) -> int32 {
    int32 first{0};
    int32 count{series.y.Num()};
    while (count > 0) {
        auto const step{count / 2};
        auto const index{first + step};
        auto const x{series.x.IsEmpty() ? static_cast<double>(index)
                                        : static_cast<double>(series.x[index])};
        if (x <= value) {
            first = index + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
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
            if (!descriptors_equal(series_[i], series[i])) {
                descriptors_changed = true;
            }
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
    auto const valid_fixed_range{[](FGraphAxisSettings const& axis) {
        return axis.range_mode != EGraphRangeMode::Fixed ||
               (FMath::IsFinite(axis.fixed_range.min) && FMath::IsFinite(axis.fixed_range.max) &&
                axis.fixed_range.min < axis.fixed_range.max);
    }};

    if (!valid_fixed_range(x_axis) || !valid_fixed_range(y_axis)) {
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
    if (!series.x.IsEmpty() && series.x.Num() != series.y.Num()) {
        ensureMsgf(false,
                   TEXT("Graph series %d has mismatched X/Y counts (%d/%d)."),
                   series_index,
                   series.x.Num(),
                   series.y.Num());
        return false;
    }

    auto const sample_count{series.y.Num()};
    float previous_x{-std::numeric_limits<float>::infinity()};
    for (int32 i{0}; i < sample_count; ++i) {
        if (!FMath::IsFinite(series.y[i])) {
            ensureMsgf(false, TEXT("Graph series %d contains non-finite Y data."), series_index);
            return false;
        }
        if (!series.x.IsEmpty()) {
            auto const x{series.x[i]};
            if (!FMath::IsFinite(x) || x < previous_x) {
                ensureMsgf(false,
                           TEXT("Graph series %d X data must be finite and non-decreasing."),
                           series_index);
                return false;
            }
            previous_x = x;
        }
    }
    return true;
}

void FGraphRenderCache::resolve_ranges(TConstArrayView<uint8> const valid_series) {
    if (x_axis_.range_mode == EGraphRangeMode::Fixed) {
        x_range_ = x_axis_.fixed_range;
    } else {
        auto min_x{std::numeric_limits<double>::infinity()};
        auto max_x{-std::numeric_limits<double>::infinity()};
        bool found_x{false};
        auto const series_count{series_.Num()};
        for (int32 series_index{0}; series_index < series_count; ++series_index) {
            if (valid_series[series_index] == 0) {
                continue;
            }
            auto const& series{series_[series_index]};
            auto const sample_count{series.y.Num()};
            if (sample_count == 0) {
                continue;
            }
            min_x = FMath::Min(min_x, sample_x(series, 0));
            max_x = FMath::Max(max_x, sample_x(series, sample_count - 1));
            found_x = true;
        }
        if (found_x && x_axis_.range_mode == EGraphRangeMode::AutoIncludeZero) {
            min_x = FMath::Min(min_x, 0.0);
            max_x = FMath::Max(max_x, 0.0);
        }
        x_range_ = expanded_auto_range(min_x, max_x);
    }

    if (y_axis_.range_mode == EGraphRangeMode::Fixed) {
        y_range_ = y_axis_.fixed_range;
        return;
    }

    auto min_y{std::numeric_limits<double>::infinity()};
    auto max_y{-std::numeric_limits<double>::infinity()};
    bool found_y{false};
    auto const series_count{series_.Num()};
    for (int32 series_index{0}; series_index < series_count; ++series_index) {
        if (valid_series[series_index] == 0) {
            continue;
        }
        auto const& series{series_[series_index]};
        auto const begin{lower_bound_x(series, x_range_.min)};
        auto const end{upper_bound_x(series, x_range_.max)};
        for (int32 sample_index{begin}; sample_index < end; ++sample_index) {
            auto const y{static_cast<double>(series.y[sample_index])};
            min_y = FMath::Min(min_y, y);
            max_y = FMath::Max(max_y, y);
            found_y = true;
        }
    }
    if (found_y && y_axis_.range_mode == EGraphRangeMode::AutoIncludeZero) {
        min_y = FMath::Min(min_y, 0.0);
        max_y = FMath::Max(max_y, 0.0);
    }
    y_range_ = expanded_auto_range(min_y, max_y);
}

void FGraphRenderCache::build_series(int32 const series_index, FVector2f const plot_size) {
    auto const& source{series_[series_index]};
    auto& cached{cached_series_[series_index]};
    cached.data_points.Reset();

    auto const sample_count{source.y.Num()};
    if (sample_count == 0) {
        cached.render_points.Reset();
        return;
    }

    auto const first_inside{lower_bound_x(source, x_range_.min)};
    auto const after_inside{upper_bound_x(source, x_range_.max)};
    auto const candidate_begin{FMath::Max(first_inside - 1, 0)};
    auto const candidate_end{FMath::Min(after_inside + 1, sample_count)};
    auto const candidate_count{candidate_end - candidate_begin};
    auto const bucket_count{FMath::Max(FMath::CeilToInt(plot_size.X), 1)};

    int32 last_sample_index{INDEX_NONE};
    auto append_sample{[&](int32 const sample_index) {
        if (sample_index == last_sample_index || !source.y.IsValidIndex(sample_index)) {
            return;
        }
        cached.data_points.Emplace(sample_x(source, sample_index),
                                   static_cast<double>(source.y[sample_index]));
        last_sample_index = sample_index;
    }};

    if (candidate_count <= bucket_count * 2) {
        cached.data_points.Reserve(candidate_count);
        for (int32 i{candidate_begin}; i < candidate_end; ++i) {
            append_sample(i);
        }
    } else {
        stats_.decimated = true;
        cached.data_points.Reserve(bucket_count * 2 + 2);
        if (first_inside > 0) {
            append_sample(first_inside - 1);
        }

        auto const x_span{x_range_.max - x_range_.min};
        int32 current_bucket{INDEX_NONE};
        int32 min_index{INDEX_NONE};
        int32 max_index{INDEX_NONE};
        float min_y{0.0f};
        float max_y{0.0f};

        auto flush_bucket{[&] {
            if (min_index == INDEX_NONE) {
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

        for (int32 i{first_inside}; i < after_inside; ++i) {
            auto const normalized_x{(sample_x(source, i) - x_range_.min) / x_span};
            auto const bucket{
                FMath::Clamp(FMath::FloorToInt(normalized_x * static_cast<double>(bucket_count)),
                             0,
                             bucket_count - 1)};
            if (bucket != current_bucket) {
                flush_bucket();
                current_bucket = bucket;
                min_index = i;
                max_index = i;
                min_y = source.y[i];
                max_y = source.y[i];
                continue;
            }
            if (source.y[i] < min_y) {
                min_y = source.y[i];
                min_index = i;
            }
            if (source.y[i] > max_y) {
                max_y = source.y[i];
                max_index = i;
            }
        }
        flush_bucket();

        if (after_inside < sample_count) {
            append_sample(after_inside);
        }
    }

    transform_series(cached, plot_size);
}

void FGraphRenderCache::transform_series(FGraphCachedSeries& series,
                                         FVector2f const plot_size) const {
    series.render_points.Reset(series.data_points.Num());
    auto const point_count{series.data_points.Num()};
    auto const render_point_count{
        series.style.interpolation == EGraphSeriesInterpolation::StepAfter && point_count > 1
            ? point_count * 2 - 1
            : point_count};
    series.render_points.Reserve(render_point_count);

    auto const x_span{x_range_.max - x_range_.min};
    auto const y_span{y_range_.max - y_range_.min};
    auto const transform_point{[&](FVector2d const& point) {
        auto const x_alpha{(point.X - x_range_.min) / x_span};
        auto const y_alpha{(point.Y - y_range_.min) / y_span};
        return FVector2f{static_cast<float>(x_alpha * plot_size.X),
                         static_cast<float>((1.0 - y_alpha) * plot_size.Y)};
    }};

    if (series.style.interpolation == EGraphSeriesInterpolation::StepAfter && point_count > 1) {
        series.render_points.Add(transform_point(series.data_points[0]));
        for (int32 i{1}; i < point_count; ++i) {
            auto const& previous{series.data_points[i - 1]};
            auto const& current{series.data_points[i]};
            series.render_points.Add(transform_point({current.X, previous.Y}));
            series.render_points.Add(transform_point(current));
        }
        return;
    }

    for (auto const& point : series.data_points) {
        series.render_points.Add(transform_point(point));
    }
}

auto FGraphRenderCache::sample_x(FGraphSeriesView const& series, int32 const sample_index)
    -> double {
    return series.x.IsEmpty() ? static_cast<double>(sample_index)
                              : static_cast<double>(series.x[sample_index]);
}

auto FGraphRenderCache::expanded_auto_range(double const min, double const max) -> FGraphRange {
    if (!FMath::IsFinite(min) || !FMath::IsFinite(max)) {
        return {};
    }
    if (min < max) {
        return {min, max};
    }
    if (min == 0.0) {
        return {-1.0, 1.0};
    }

    auto const padding{FMath::Max(FMath::Abs(min) * 0.05,
                                  std::numeric_limits<double>::epsilon() * FMath::Abs(min) * 16.0)};
    return {min - padding, max + padding};
}
