#include "sandbox/core/graph_plot.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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
