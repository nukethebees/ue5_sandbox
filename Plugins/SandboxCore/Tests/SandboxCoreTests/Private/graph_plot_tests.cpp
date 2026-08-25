#include <SandboxCore/graph_plot.h>

#include "CoreMinimal.h"
#include "TestHarness.h"
#include "Tests/EnsureScope.h"

#include <limits>

namespace {
auto make_series(TConstArrayView<float> const x, TConstArrayView<float> const y) -> FGraphSeriesView {
    return {
        .name = FText::FromString(TEXT("test")),
        .x = x,
        .y = y,
        .style = {.color = FLinearColor::Green},
    };
}

void set_one_series(FGraphRenderCache& cache, FGraphSeriesView const& series, uint64 const revision) {
    CHECK(cache.set_series(TConstArrayView<FGraphSeriesView>{&series, 1}, revision));
}
}

TEST_CASE("SandboxCore.GraphPlot.Empty data uses stable default ranges") {
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series({}, {})};
    set_one_series(cache, series, 1);

    CHECK(cache.update({640.0f, 360.0f}));
    CHECK(cache.get_x_range() == FGraphRange{0.0, 1.0});
    CHECK(cache.get_y_range() == FGraphRange{0.0, 1.0});
    REQUIRE(cache.get_series().Num() == 1);
    CHECK(cache.get_series()[0].render_points.IsEmpty());
}

TEST_CASE("SandboxCore.GraphPlot.Implicit X and automatic ranges transform samples") {
    TArray<float> const y{2.0f, 4.0f, 3.0f};
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series({}, y)};
    set_one_series(cache, series, 1);

    CHECK(cache.update({200.0f, 100.0f}));
    CHECK(cache.get_x_range() == FGraphRange{0.0, 2.0});
    CHECK(cache.get_y_range() == FGraphRange{2.0, 4.0});
    auto const points{cache.get_series()[0].render_points};
    REQUIRE(points.Num() == 3);
    CHECK(points[0].Equals({0.0f, 100.0f}));
    CHECK(points[1].Equals({100.0f, 0.0f}));
    CHECK(points[2].Equals({200.0f, 50.0f}));
}

TEST_CASE("SandboxCore.GraphPlot.Auto Y considers only the fixed X window") {
    TArray<float> const x{0.0f, 1.0f, 2.0f, 3.0f};
    TArray<float> const y{100.0f, 2.0f, 3.0f, 200.0f};
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series(x, y)};
    set_one_series(cache, series, 1);
    CHECK(cache.set_axis_settings({.range_mode = EGraphRangeMode::Fixed, .fixed_range = {0.5, 2.5}}, {}));

    CHECK(cache.update({200.0f, 100.0f}));
    CHECK(cache.get_x_range() == FGraphRange{0.5, 2.5});
    CHECK(cache.get_y_range() == FGraphRange{2.0, 3.0});

    auto const points{cache.get_series()[0].render_points};
    REQUIRE(points.Num() == 4);
    CHECK(points[0].X < 0.0f);
    CHECK(points.Last().X > 200.0f);
}

TEST_CASE("SandboxCore.GraphPlot.Include-zero and constant data produce usable ranges") {
    TArray<float> const y{5.0f, 5.0f, 5.0f};
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series({}, y)};
    set_one_series(cache, series, 1);
    CHECK(cache.set_axis_settings({}, {.range_mode = EGraphRangeMode::AutoIncludeZero}));

    CHECK(cache.update({100.0f, 100.0f}));
    CHECK(cache.get_y_range() == FGraphRange{0.0, 5.0});
}

TEST_CASE("SandboxCore.GraphPlot.One sample is retained for marker rendering") {
    TArray<float> const x{1.0e30f};
    TArray<float> const y{1.0e-30f};
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series(x, y)};
    set_one_series(cache, series, 1);

    CHECK(cache.update({100.0f, 100.0f}));
    REQUIRE(cache.get_series()[0].render_points.Num() == 1);
    CHECK(FMath::IsFinite(cache.get_series()[0].render_points[0].X));
    CHECK(FMath::IsFinite(cache.get_series()[0].render_points[0].Y));
}

TEST_CASE("SandboxCore.GraphPlot.Pixel extrema decimation preserves a narrow spike") {
    int32 constexpr sample_count{10000};
    int32 constexpr pixel_width{100};
    TArray<float> y;
    y.Init(0.0f, sample_count);
    y[4321] = 100.0f;

    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series({}, y)};
    set_one_series(cache, series, 1);
    CHECK(cache.update({static_cast<float>(pixel_width), 100.0f}));

    auto const points{cache.get_series()[0].render_points};
    CHECK(cache.get_stats().decimated);
    CHECK(points.Num() <= pixel_width * 2 + 2);
    CHECK(points.ContainsByPredicate([](FVector2f const point) { return point.Y == 0.0f; }));
}

TEST_CASE("SandboxCore.GraphPlot.Cache rebuilds only when its key changes") {
    TArray<float> y{1.0f, 2.0f, 3.0f};
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series({}, y)};
    set_one_series(cache, series, 1);

    CHECK(cache.update({100.0f, 50.0f}));
    auto const initial_rebuilds{cache.get_stats().rebuild_count};
    CHECK_FALSE(cache.update({100.0f, 50.0f}));
    CHECK(cache.get_stats().rebuild_count == initial_rebuilds);

    CHECK(cache.update({200.0f, 50.0f}));
    CHECK(cache.get_stats().rebuild_count == initial_rebuilds + 1);

    y[1] = 20.0f;
    set_one_series(cache, series, 2);
    CHECK(cache.update({200.0f, 50.0f}));
    CHECK(cache.get_y_range().max == 20.0);
}

TEST_CASE("SandboxCore.GraphPlot.Multiple series share automatic ranges") {
    TArray<float> const y_a{-2.0f, 1.0f};
    TArray<float> const y_b{3.0f, 8.0f, 4.0f};
    TArray<FGraphSeriesView> const series{make_series({}, y_a), make_series({}, y_b)};
    FGraphRenderCache cache;
    CHECK(cache.set_series(series, 1));

    CHECK(cache.update({100.0f, 100.0f}));
    CHECK(cache.get_x_range() == FGraphRange{0.0, 2.0});
    CHECK(cache.get_y_range() == FGraphRange{-2.0, 8.0});
    CHECK(cache.get_series().Num() == 2);
}

TEST_CASE("SandboxCore.GraphPlot.Rejects stale revisions without replacing cached data") {
    TArray<float> const original_y{1.0f, 2.0f, 3.0f};
    TArray<float> const stale_y{100.0f, 200.0f, 300.0f};
    FGraphRenderCache cache;
    FGraphSeriesView const original_series{make_series({}, original_y)};
    FGraphSeriesView const stale_series{make_series({}, stale_y)};
    set_one_series(cache, original_series, 5);
    REQUIRE(cache.update({100.0f, 100.0f}));
    auto const original_points{cache.get_series()[0].render_points};

    FEnsureScope ensure_scope;
    CHECK_FALSE(cache.set_series(TConstArrayView<FGraphSeriesView>{&stale_series, 1}, 4));
    CHECK(ensure_scope.GetCount() == 1);
    CHECK_FALSE(cache.update({100.0f, 100.0f}));
    CHECK(cache.get_y_range() == FGraphRange{1.0, 3.0});
    CHECK(cache.get_series()[0].render_points == original_points);
}

TEST_CASE("SandboxCore.GraphPlot.Invalid series do not suppress valid siblings") {
    TArray<float> const valid_y{2.0f, 4.0f, 3.0f};
    auto const check_invalid_series{[&](TConstArrayView<float> const invalid_x, TConstArrayView<float> const invalid_y) {
        TArray<FGraphSeriesView> const series{make_series({}, valid_y), make_series(invalid_x, invalid_y)};
        FGraphRenderCache cache;
        REQUIRE(cache.set_series(series, 1));

        FEnsureScope ensure_scope;
        CHECK(cache.update({100.0f, 100.0f}));
        CHECK(cache.get_x_range() == FGraphRange{0.0, 2.0});
        CHECK(cache.get_y_range() == FGraphRange{2.0, 4.0});
        CHECK(cache.get_stats().source_sample_count == valid_y.Num());
        REQUIRE(cache.get_series().Num() == 2);
        CHECK_FALSE(cache.get_series()[0].render_points.IsEmpty());
        CHECK(cache.get_series()[1].render_points.IsEmpty());
    }};

    SECTION("mismatched X and Y counts") {
        TArray<float> const x{0.0f, 1.0f};
        TArray<float> const y{100.0f, 200.0f, 300.0f};
        check_invalid_series(x, y);
    }
    SECTION("decreasing X values") {
        TArray<float> const x{0.0f, 2.0f, 1.0f};
        TArray<float> const y{100.0f, 200.0f, 300.0f};
        check_invalid_series(x, y);
    }
    SECTION("non-finite X value") {
        TArray<float> const x{0.0f, std::numeric_limits<float>::infinity(), 2.0f};
        TArray<float> const y{100.0f, 200.0f, 300.0f};
        check_invalid_series(x, y);
    }
    SECTION("non-finite Y value") {
        TArray<float> const x{0.0f, 1.0f, 2.0f};
        TArray<float> const y{100.0f, std::numeric_limits<float>::quiet_NaN(), 300.0f};
        check_invalid_series(x, y);
    }
}

TEST_CASE("SandboxCore.GraphPlot.Rejects invalid fixed ranges without changing valid settings") {
    TArray<float> const y{1.0f, 2.0f, 3.0f};
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series({}, y)};
    set_one_series(cache, series, 1);
    REQUIRE(cache.set_axis_settings({.range_mode = EGraphRangeMode::Fixed, .fixed_range = {0.0, 10.0}},
                                    {.range_mode = EGraphRangeMode::Fixed, .fixed_range = {-5.0, 5.0}}));
    REQUIRE(cache.update({100.0f, 100.0f}));

    auto const check_invalid_range{[&](FGraphRange const invalid_range) {
        FEnsureScope ensure_scope;
        CHECK_FALSE(cache.set_axis_settings({.range_mode = EGraphRangeMode::Fixed, .fixed_range = invalid_range}, {}));
        CHECK(cache.update({200.0f, 100.0f}));
        CHECK(cache.get_x_range() == FGraphRange{0.0, 10.0});
        CHECK(cache.get_y_range() == FGraphRange{-5.0, 5.0});
    }};

    SECTION("equal bounds") {
        check_invalid_range({2.0, 2.0});
    }
    SECTION("reversed bounds") {
        check_invalid_range({3.0, 2.0});
    }
    SECTION("non-finite bound") {
        check_invalid_range({0.0, std::numeric_limits<double>::infinity()});
    }
}

TEST_CASE("SandboxCore.GraphPlot.Fixed windows retain samples across both plot edges") {
    TArray<float> const x{0.0f, 1.0f, 2.0f, 3.0f};
    TArray<float> const y{0.0f, 1.0f, 2.0f, 3.0f};
    FGraphRenderCache cache;
    FGraphSeriesView const series{make_series(x, y)};
    set_one_series(cache, series, 1);
    REQUIRE(cache.set_axis_settings({.range_mode = EGraphRangeMode::Fixed, .fixed_range = {1.25, 1.75}},
                                    {.range_mode = EGraphRangeMode::Fixed, .fixed_range = {0.0, 3.0}}));

    REQUIRE(cache.update({100.0f, 100.0f}));
    auto const& cached_series{cache.get_series()[0]};
    REQUIRE(cached_series.data_points.Num() == 2);
    REQUIRE(cached_series.render_points.Num() == 2);
    CHECK(cached_series.data_points[0] == FVector2d{1.0, 1.0});
    CHECK(cached_series.data_points[1] == FVector2d{2.0, 2.0});
    CHECK(cached_series.render_points[0].X < 0.0f);
    CHECK(cached_series.render_points[1].X > 100.0f);
}
