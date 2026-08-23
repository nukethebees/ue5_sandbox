#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"

enum class EGraphRangeMode : uint8 {
    Auto,
    AutoIncludeZero,
    Fixed,
};

struct SANDBOXCORE_API FGraphRange {
    double min{0.0};
    double max{1.0};

    bool operator==(FGraphRange const&) const = default;
};

struct SANDBOXCORE_API FGraphAxisSettings {
    EGraphRangeMode range_mode{EGraphRangeMode::Auto};
    FGraphRange fixed_range{};

    bool operator==(FGraphAxisSettings const&) const = default;
};

struct SANDBOXCORE_API FGraphSeriesStyle {
    FLinearColor color{FLinearColor::White};
    float thickness{1.0f};
    bool antialias{false};

    bool operator==(FGraphSeriesStyle const&) const = default;
};

/**
 * Non-owning views over one ordered graph series. The caller owns x and y and must keep their
 * storage valid until the series is replaced or the consuming graph is destroyed.
 */
struct SANDBOXCORE_API FGraphSeriesView {
    FText name;

    // Empty x means implicit x = sample index.
    TConstArrayView<float> x;
    TConstArrayView<float> y;

    FGraphSeriesStyle style;
};

struct SANDBOXCORE_API FGraphCachedSeries {
    FText name;
    FGraphSeriesStyle style;
    TArray<FVector2d> data_points;
    TArray<FVector2f> render_points;
};

struct SANDBOXCORE_API FGraphCacheStats {
    uint64 rebuild_count{0};
    int64 source_sample_count{0};
    int32 emitted_point_count{0};
    bool decimated{false};
};

/** Resolution-bounded data/range cache shared by Slate and low-level tests/benchmarks. */
class SANDBOXCORE_API FGraphRenderCache {
  public:
    /** Copies series descriptors and metadata, but never copies source sample arrays. */
    [[nodiscard]] bool set_series(TConstArrayView<FGraphSeriesView> series, uint64 data_revision);
    [[nodiscard]] bool set_axis_settings(FGraphAxisSettings x_axis, FGraphAxisSettings y_axis);

    /** Rebuilds dirty cache data for the supplied plot-local size. */
    [[nodiscard]] bool update(FVector2f plot_size);

    auto get_series() const noexcept -> TConstArrayView<FGraphCachedSeries> {
        return cached_series_;
    }
    auto get_x_range() const noexcept -> FGraphRange { return x_range_; }
    auto get_y_range() const noexcept -> FGraphRange { return y_range_; }
    auto get_stats() const noexcept -> FGraphCacheStats const& { return stats_; }
  private:
    bool validate_series(FGraphSeriesView const& series, int32 series_index) const;
    void resolve_ranges(TConstArrayView<uint8> valid_series);
    void build_series(int32 series_index, FVector2f plot_size);
    void transform_series(FGraphCachedSeries& series, FVector2f plot_size) const;

    static auto sample_x(FGraphSeriesView const& series, int32 sample_index) -> double;
    static auto expanded_auto_range(double min, double max) -> FGraphRange;

    TArray<FGraphSeriesView> series_;
    TArray<FGraphCachedSeries> cached_series_;
    TArray<uint8> valid_series_;
    FGraphAxisSettings x_axis_;
    FGraphAxisSettings y_axis_;
    FGraphRange x_range_;
    FGraphRange y_range_;
    FGraphCacheStats stats_;
    FVector2f plot_size_{0.0f, 0.0f};
    uint64 data_revision_{0};
    bool has_data_revision_{false};
    bool dirty_{true};
};
