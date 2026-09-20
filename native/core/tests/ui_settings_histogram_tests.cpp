#include "sandbox/core/ui/chart_layout.h"
#include "sandbox/core/ui/histogram.h"
#include "sandbox/core/ui/settings_slider.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <vector>

TEST(SettingsSlider, ConvertsClampsAndStepsConfiguredRange) {
    using namespace ml::ui::settings_slider;
    EXPECT_FLOAT_EQ(normalize(50.0f, 50.0f, 100.0f), 0.0f);
    EXPECT_FLOAT_EQ(normalize(100.0f, 50.0f, 100.0f), 1.0f);
    EXPECT_FLOAT_EQ(normalize(10.0f, 50.0f, 100.0f), 0.0f);
    EXPECT_FLOAT_EQ(normalized_step(5.0f, 50.0f, 100.0f), 0.1f);
    EXPECT_FLOAT_EQ(normalized_step(5.0f, 50.0f, 50.0f), 1.0f);
    EXPECT_FLOAT_EQ(denormalize(0.506f, 0.0f, 1.0f, 0.01f), 0.51f);
    EXPECT_FLOAT_EQ(denormalize(2.0f, 50.0f, 100.0f, 1.0f), 100.0f);
}

TEST(ChartLayout, ReservesPaddingAxisAndLabelArea) {
    auto const layout{ml::ui::chart_layout::make_layout({200.0f, 100.0f},
                                                        {.padding = {10.0f, 10.0f, 10.0f, 10.0f},
                                                         .axis_thickness = 1.0f,
                                                         .label_area_height = 20.0f})};
    EXPECT_EQ(layout.plot_origin, (ml::ui::Vector2f{11.0f, 10.0f}));
    EXPECT_EQ(layout.plot_size, (ml::ui::Vector2f{179.0f, 59.0f}));
    EXPECT_FLOAT_EQ(layout.label_area_height, 20.0f);

    auto const collapsed{
        ml::ui::chart_layout::make_layout({5.0f, 5.0f}, {.padding = {10.0f, 10.0f, 10.0f, 10.0f}})};
    EXPECT_EQ(collapsed.plot_size, (ml::ui::Vector2f{}));
    EXPECT_FLOAT_EQ(collapsed.label_area_height, 0.0f);
}

TEST(Histogram, AssignsSamplesToFixedDomainBins) {
    std::array const samples{0.0f, 0.99f, 1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_EQ(ml::ui::histogram::build_bins(samples, 0.0f, 4.0f, 4),
              (std::vector<std::int32_t>{2, 1, 1, 2}));
}

TEST(Histogram, IgnoresOutOfDomainAndNonFiniteSamples) {
    std::array const samples{-0.1f,
                             0.25f,
                             0.75f,
                             1.1f,
                             std::numeric_limits<float>::quiet_NaN(),
                             std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity()};
    EXPECT_EQ(ml::ui::histogram::build_bins(samples, 0.0f, 1.0f, 2),
              (std::vector<std::int32_t>{1, 1}));
}

TEST(Histogram, HandlesEmptyAndInvalidConfigurations) {
    using namespace ml::ui::histogram;
    auto const empty{build_bins({}, 0.0f, 1.0f, 4)};
    EXPECT_EQ(empty.size(), 4);
    EXPECT_EQ(maximum_bin_count(empty), 0);
    EXPECT_TRUE(build_bins({}, 1.0f, 0.0f, 4).empty());
    EXPECT_TRUE(build_bins({}, 1.0f, 1.0f, 4).empty());
    EXPECT_TRUE(build_bins({}, 0.0f, 1.0f, 0).empty());
    EXPECT_TRUE(build_bins({}, 0.0f, std::numeric_limits<float>::infinity(), 4).empty());
}

TEST(Histogram, BuildsProportionalBarsAndStableEmptyGeometry) {
    using namespace ml::ui::histogram;
    std::array const bins{2, 4, 0};
    auto const geometry{build_geometry(bins, {120.0f, 100.0f}, 4.0f)};
    ASSERT_EQ(geometry.bars.size(), 2);
    EXPECT_EQ(geometry.maximum_count, 4);
    EXPECT_FLOAT_EQ(geometry.slot_width, 40.0f);
    EXPECT_FLOAT_EQ(geometry.bar_width, 36.0f);
    EXPECT_EQ(geometry.bars[0].bin_index, 0);
    EXPECT_EQ(geometry.bars[0].count, 2);
    EXPECT_FLOAT_EQ(geometry.bars[0].size.y, 50.0f);
    EXPECT_FLOAT_EQ(geometry.bars[0].position.y, 50.0f);
    EXPECT_FLOAT_EQ(geometry.bars[1].size.y, 100.0f);
    EXPECT_FLOAT_EQ(geometry.bars[1].position.y, 0.0f);

    std::array const zero_bins{0, 0};
    auto const zero{build_geometry(zero_bins, {100.0f, 100.0f}, 2.0f)};
    EXPECT_TRUE(zero.bars.empty());
    EXPECT_FLOAT_EQ(zero.slot_width, 50.0f);
}

TEST(Histogram, HitTestsBinsDeterministically) {
    using ml::ui::histogram::hit_test_bin;
    EXPECT_FALSE(hit_test_bin({9.0f, 20.0f}, {10.0f, 10.0f}, {100.0f, 50.0f}, 4));
    EXPECT_EQ(hit_test_bin({10.0f, 20.0f}, {10.0f, 10.0f}, {100.0f, 50.0f}, 4), 0);
    EXPECT_EQ(hit_test_bin({61.0f, 20.0f}, {10.0f, 10.0f}, {100.0f, 50.0f}, 4), 2);

    auto const range{ml::ui::histogram::bin_range(10.0f, 20.0f, 5, 2)};
    ASSERT_TRUE(range);
    EXPECT_FLOAT_EQ(range->minimum, 14.0f);
    EXPECT_FLOAT_EQ(range->maximum, 16.0f);
    EXPECT_FALSE(ml::ui::histogram::bin_range(10.0f, 20.0f, 5, 5));
}

TEST(Histogram, OwnsRebinsRejectsAndClearsSamples) {
    ml::ui::histogram::Data data;
    ASSERT_TRUE(data.set_configuration(0.0f, 4.0f, 4));
    std::vector<float> samples{0.5f, 1.5f};
    data.set_samples(samples);
    samples[0] = 3.5f;
    EXPECT_FLOAT_EQ(data.samples()[0], 0.5f);
    EXPECT_EQ(data.bins()[0], 1);

    data.set_samples({2.5f, 2.75f});
    EXPECT_EQ(data.bins()[2], 2);
    ASSERT_TRUE(data.set_configuration(2.0f, 3.0f, 2));
    EXPECT_EQ(data.bins()[1], 2);
    EXPECT_FALSE(data.set_configuration(3.0f, 2.0f, 0));
    EXPECT_EQ(data.bin_count(), 2);
    data.clear_samples();
    EXPECT_TRUE(data.samples().empty());
    EXPECT_EQ(data.bins().size(), 2);
    EXPECT_EQ(ml::ui::histogram::maximum_bin_count(data.bins()), 0);
}
