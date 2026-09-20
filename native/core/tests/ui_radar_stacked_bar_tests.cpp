#include "sandbox/core/ui/radar_2d.h"
#include "sandbox/core/ui/stacked_bar_chart.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

TEST(Radar2D, MapsCentreEdgesRangesAndAspectRatios) {
    using namespace ml::ui::radar_2d;
    auto const layout{make_layout({200.0f, 100.0f}, 10.0f)};
    EXPECT_EQ(layout.origin, (ml::ui::Vector2f{50.0f, 0.0f}));
    EXPECT_EQ(layout.size, (ml::ui::Vector2f{100.0f, 100.0f}));
    EXPECT_EQ(to_local({0.0f, 0.0f}, layout), (ml::ui::Vector2f{100.0f, 50.0f}));
    EXPECT_EQ(to_local({10.0f, 0.0f}, layout), (ml::ui::Vector2f{150.0f, 50.0f}));
    EXPECT_EQ(to_local({-10.0f, 0.0f}, layout), (ml::ui::Vector2f{50.0f, 50.0f}));
    EXPECT_EQ(to_local({0.0f, 10.0f}, layout), (ml::ui::Vector2f{100.0f, 0.0f}));
    EXPECT_EQ(to_local({0.0f, -10.0f}, layout), (ml::ui::Vector2f{100.0f, 100.0f}));

    auto const square{make_layout({100.0f, 100.0f}, 20.0f)};
    auto const tall{make_layout({100.0f, 200.0f}, 20.0f)};
    EXPECT_FLOAT_EQ(square.pixels_per_unit, 2.5f);
    EXPECT_EQ(to_local({10.0f, 0.0f}, square), (ml::ui::Vector2f{75.0f, 50.0f}));
    EXPECT_EQ(tall.origin, (ml::ui::Vector2f{0.0f, 50.0f}));
    EXPECT_EQ(tall.centre, (ml::ui::Vector2f{50.0f, 100.0f}));
}

TEST(Radar2D, HandlesInvalidLayoutInputsSafely) {
    using namespace ml::ui::radar_2d;
    auto const zero{make_layout({0.0f, 100.0f}, 10.0f)};
    auto const invalid{make_layout({100.0f, 100.0f}, 0.0f)};
    EXPECT_FLOAT_EQ(zero.pixels_per_unit, 0.0f);
    EXPECT_FLOAT_EQ(invalid.pixels_per_unit, 0.0f);
    EXPECT_EQ(to_local({10.0f, 10.0f}, zero), zero.centre);
    EXPECT_FALSE(is_valid_extent({0.0f, 6.0f}));
    EXPECT_TRUE(is_valid_extent({6.0f, 6.0f}));
}

TEST(Radar2D, OwnsReplacesRejectsAndClearsPositionBuckets) {
    using namespace ml::ui::radar_2d;
    Data data;
    EXPECT_FALSE(data.set_range(0.0f));
    EXPECT_FLOAT_EQ(data.range(), 1.0f);
    auto const first{data.add_bucket()};
    EXPECT_EQ(first, 0);
    Positions positions;
    positions.add(1.0f, 2.0f);
    positions.add(3.0f, 4.0f);
    ASSERT_TRUE(data.set_positions(first, positions));
    positions.xs[0] = 100.0f;
    EXPECT_FLOAT_EQ(data.buckets()[0].xs[0], 1.0f);
    EXPECT_FALSE(data.set_positions(1, {}));
    Positions invalid;
    invalid.xs.push_back(1.0f);
    EXPECT_FALSE(data.set_positions(first, std::move(invalid)));
    EXPECT_EQ(data.buckets()[0].size(), 2);
    EXPECT_TRUE(data.clear_positions(first));
    EXPECT_TRUE(data.buckets()[0].empty());
    data.clear_buckets();
    EXPECT_TRUE(data.buckets().empty());
}

TEST(StackedBarChart, CalculatesPositiveTotalsAndMaximum) {
    using namespace ml::ui::stacked_bar_chart;
    Bar first{10.0f,
              -5.0f,
              20.0f,
              0.0f,
              std::numeric_limits<float>::quiet_NaN(),
              std::numeric_limits<float>::infinity()};
    std::vector<Bar> const bars{first, {5.0f, 15.0f}};
    EXPECT_FLOAT_EQ(bar_total(bars[0]), 30.0f);
    EXPECT_FLOAT_EQ(maximum_total(bars), 30.0f);
}

TEST(StackedBarChart, BuildsBottomUpOrderedGeometry) {
    using namespace ml::ui::stacked_bar_chart;
    std::vector<Bar> const bars{{10.0f, 20.0f}, {15.0f}};
    auto const geometry{build_geometry(bars, {100.0f, 120.0f}, 10.0f)};
    ASSERT_EQ(geometry.segments.size(), 3);
    EXPECT_FLOAT_EQ(geometry.maximum_total, 30.0f);
    EXPECT_FLOAT_EQ(geometry.slot_width, 50.0f);
    EXPECT_FLOAT_EQ(geometry.bar_width, 40.0f);
    EXPECT_EQ(geometry.segments[0].segment_index, 0);
    EXPECT_FLOAT_EQ(geometry.segments[0].size.y, 40.0f);
    EXPECT_FLOAT_EQ(geometry.segments[0].position.y, 80.0f);
    EXPECT_EQ(geometry.segments[1].segment_index, 1);
    EXPECT_FLOAT_EQ(geometry.segments[1].size.y, 80.0f);
    EXPECT_FLOAT_EQ(geometry.segments[1].position.y, 0.0f);
    EXPECT_FLOAT_EQ(geometry.segments[2].size.y, 60.0f);
    EXPECT_FLOAT_EQ(geometry.segments[2].position.y, 60.0f);
}

TEST(StackedBarChart, SkipsInvalidSegmentsAndHandlesZeroData) {
    using namespace ml::ui::stacked_bar_chart;
    std::vector<Bar> const bars{{-10.0f, 5.0f, -1.0f, 5.0f}};
    auto const geometry{build_geometry(bars, {20.0f, 100.0f}, 0.0f)};
    ASSERT_EQ(geometry.segments.size(), 2);
    EXPECT_EQ(geometry.segments[0].segment_index, 1);
    EXPECT_EQ(geometry.segments[1].segment_index, 3);
    EXPECT_FLOAT_EQ(geometry.segments[0].position.y, 50.0f);
    EXPECT_FLOAT_EQ(geometry.segments[1].position.y, 0.0f);

    std::vector<Bar> const zero{{0.0f, -2.0f}};
    auto const empty_geometry{build_geometry(zero, {100.0f, 100.0f}, 8.0f)};
    EXPECT_TRUE(empty_geometry.segments.empty());
    EXPECT_FLOAT_EQ(empty_geometry.slot_width, 100.0f);
}

TEST(StackedBarChart, OwnsReplacesAndClearsBars) {
    using namespace ml::ui::stacked_bar_chart;
    Data data;
    std::vector<Bar> bars{{1.0f}};
    data.set_bars(bars);
    bars[0][0] = 100.0f;
    EXPECT_FLOAT_EQ(data.bars()[0][0], 1.0f);
    data.set_bars({{2.0f}, {3.0f}});
    EXPECT_EQ(data.bars().size(), 2);
    data.clear_bars();
    EXPECT_TRUE(data.bars().empty());
}
