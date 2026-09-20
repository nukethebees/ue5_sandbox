#include "sandbox/core/graph_plot.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

TEST(GraphPlot, ResolvesRangesAndTransformsImplicitSeries) {
    std::array const y{10.0f, 20.0f, 30.0f};
    std::array const series{ml::graph::SeriesView{.x = {}, .y = y}};
    std::array const valid{std::uint8_t{1}};

    auto const x_range{ml::graph::resolve_x_range(series, valid, {})};
    auto const y_range{ml::graph::resolve_y_range(series, valid, {}, x_range)};
    EXPECT_EQ(x_range, (ml::graph::Range{0.0, 2.0}));
    EXPECT_EQ(y_range, (ml::graph::Range{10.0, 30.0}));

    bool decimated{false};
    auto const points{ml::graph::build_data_points(series[0], x_range, 100.0f, decimated)};
    auto const rendered{ml::graph::transform_points(
        points, x_range, y_range, 100.0f, 50.0f, ml::graph::Interpolation::Linear)};
    ASSERT_EQ(rendered.size(), 3);
    EXPECT_EQ(rendered.front().x, 0.0f);
    EXPECT_EQ(rendered.front().y, 50.0f);
    EXPECT_EQ(rendered.back().x, 100.0f);
    EXPECT_EQ(rendered.back().y, 0.0f);
    EXPECT_FALSE(decimated);
}

TEST(GraphPlot, StepAfterAddsTransitionPoints) {
    std::array const points{ml::graph::Point2d{0.0, 2.0}, ml::graph::Point2d{1.0, 4.0}};
    auto const rendered{ml::graph::transform_points(
        points, {0.0, 1.0}, {2.0, 4.0}, 100.0f, 50.0f, ml::graph::Interpolation::StepAfter)};
    ASSERT_EQ(rendered.size(), 3);
    EXPECT_EQ(rendered[1].x, 100.0f);
    EXPECT_EQ(rendered[1].y, 50.0f);
    EXPECT_EQ(rendered[2].y, 0.0f);
}

TEST(GraphPlot, DecimationPreservesNarrowSpike) {
    std::vector<float> x(1000);
    std::vector<float> y(1000, 0.0f);
    for (std::size_t i{}; i < x.size(); ++i) {
        x[i] = static_cast<float>(i);
    }
    y[501] = 100.0f;

    bool decimated{false};
    auto const points{ml::graph::build_data_points({x, y}, {0.0, 999.0}, 32.0f, decimated)};
    EXPECT_TRUE(decimated);
    EXPECT_LE(points.size(), 66);
    EXPECT_NE(std::ranges::find(points, ml::graph::Point2d{501.0, 100.0}), points.end());
}

TEST(GraphPlot, ResolvesYRangeInsideTheFixedXWindow) {
    std::array const x{0.0f, 1.0f, 2.0f, 3.0f};
    std::array const y{100.0f, 2.0f, 3.0f, 200.0f};
    std::array const series{ml::graph::SeriesView{.x = x, .y = y}};
    std::array const valid{std::uint8_t{1}};
    auto const axis{ml::graph::AxisSettings{.range_mode = ml::graph::RangeMode::Fixed,
                                            .fixed_range = {0.5, 2.5}}};

    EXPECT_EQ(ml::graph::resolve_y_range(series, valid, {}, axis.fixed_range),
              (ml::graph::Range{2.0, 3.0}));
}

TEST(GraphPlot, HandlesConstantIncludeZeroAndInvalidRanges) {
    std::array const y{5.0f, 5.0f};
    std::array const series{ml::graph::SeriesView{.x = {}, .y = y}};
    std::array const valid{std::uint8_t{1}};
    auto const include_zero{
        ml::graph::AxisSettings{.range_mode = ml::graph::RangeMode::AutoIncludeZero}};

    EXPECT_EQ(ml::graph::resolve_y_range(series, valid, include_zero, {0.0, 1.0}),
              (ml::graph::Range{0.0, 5.0}));
    EXPECT_EQ(ml::graph::resolve_y_range(series, valid, {}, {0.0, 1.0}),
              (ml::graph::Range{4.75, 5.25}));
    EXPECT_FALSE(ml::graph::is_valid_fixed_range(
        {.range_mode = ml::graph::RangeMode::Fixed, .fixed_range = {2.0, 2.0}}));
    EXPECT_FALSE(ml::graph::is_valid_fixed_range(
        {.range_mode = ml::graph::RangeMode::Fixed,
         .fixed_range = {0.0, std::numeric_limits<double>::infinity()}}));
}

TEST(GraphPlot, RetainsNeighboursAcrossFixedWindowEdges) {
    std::array const x{0.0f, 1.0f, 2.0f, 3.0f};
    std::array const y{0.0f, 1.0f, 2.0f, 3.0f};
    bool decimated{false};

    auto const points{ml::graph::build_data_points({x, y}, {1.25, 1.75}, 100.0f, decimated)};

    EXPECT_EQ(points, (std::vector<ml::graph::Point2d>{{1.0, 1.0}, {2.0, 2.0}}));
    EXPECT_FALSE(decimated);
}

TEST(GraphPlot, FindsNearestVisibleXAcrossSeries) {
    std::array const first_x{0.0f, 2.0f, 5.0f};
    std::array const first_y{1.0f, 2.0f, 3.0f};
    std::array const second_y{4.0f, 5.0f};
    std::array const series{ml::graph::SeriesView{first_x, first_y},
                            ml::graph::SeriesView{{}, second_y}};
    EXPECT_EQ(ml::graph::nearest_x(series, 3.1), 2.0);
    EXPECT_EQ(ml::graph::nearest_sample_index(series[0], 3.1), 1);

    std::array const non_finite_x{std::numeric_limits<float>::quiet_NaN(), 4.0f};
    EXPECT_EQ(ml::graph::nearest_sample_index({non_finite_x, second_y}, 3.1), 1);
    EXPECT_FALSE(ml::graph::nearest_sample_index({first_x, second_y}, 3.1));

    std::array const tied_x{0.0f, 2.0f};
    EXPECT_EQ(ml::graph::nearest_sample_index({tied_x, second_y}, 1.0), 0);
    std::array const tied_series{ml::graph::SeriesView{tied_x, second_y}};
    EXPECT_EQ(ml::graph::nearest_x(tied_series, 1.0), 0.0);
}

TEST(GraphPlot, ValidatesPortableLayoutSettings) {
    EXPECT_TRUE(ml::graph::is_valid_layout({}));
    auto invalid{ml::graph::LayoutSettings{}};
    invalid.left_margin = -1.0f;
    EXPECT_FALSE(ml::graph::is_valid_layout(invalid));

    auto const layout{ml::graph::make_plot_layout({320.0f, 180.0f}, {})};
    EXPECT_EQ(layout.origin.x, 54.0f);
    EXPECT_EQ(layout.origin.y, 12.0f);
    EXPECT_EQ(layout.size.x, 254.0f);
    EXPECT_EQ(layout.size.y, 142.0f);
}

TEST(GraphPlot, BuildsReadableTicksAndSupportsInversion) {
    auto const ticks{ml::graph::build_ticks({0.0, 10.0}, 100.0f, 5, false)};
    ASSERT_EQ(ticks.size(), 6);
    EXPECT_EQ(ticks.front().value, 0.0);
    EXPECT_EQ(ticks.front().position, 0.0f);
    EXPECT_EQ(ticks.back().value, 10.0);
    EXPECT_EQ(ticks.back().position, 100.0f);

    auto const inverted{ml::graph::build_ticks({0.0, 10.0}, 100.0f, 5, true)};
    EXPECT_EQ(inverted.front().position, 100.0f);
    EXPECT_EQ(inverted.back().position, 0.0f);
    EXPECT_TRUE(ml::graph::build_ticks({1.0, 1.0}, 100.0f, 5, false).empty());
}
