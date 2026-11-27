#include "ioj/ui/entity_overlay.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

TEST(EntityOverlay, ValidatesSourceSizes) {
    using ioj::ui::entity_overlay::source_sizes_match;
    EXPECT_TRUE(source_sizes_match(0, 0, 0));
    EXPECT_TRUE(source_sizes_match(2, 2, 2));
    EXPECT_FALSE(source_sizes_match(1, 0, 0));
}

TEST(EntityOverlay, PacksClampedFillColor) {
    using namespace ioj::ui::entity_overlay;
    auto const packed{pack_display_data({-1.0f, 0.5f, 2.0f, 0.0f})};
    EXPECT_NE(packed & (1U << 2U), 0U);
    EXPECT_EQ((packed >> 8U) & 0xffU, 0U);
    EXPECT_EQ((packed >> 16U) & 0xffU, 128U);
    EXPECT_EQ((packed >> 24U) & 0xffU, 255U);
}

TEST(EntityOverlay, FiltersRangeInclusivelyAndCompacts) {
    using namespace ioj::ui::entity_overlay;
    Collector collector;
    collector.begin({}, 10.0f);
    std::vector<Instance> output;
    auto inside{collector.try_add({9.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 0)};
    auto boundary{collector.try_add({10.0f, 0.0f, 0.0f}, 0.5f, 1.0f, 0)};
    EXPECT_FALSE(collector.try_add({10.01f, 0.0f, 0.0f}, 1.0f, 1.0f, 0));
    ASSERT_TRUE(inside);
    ASSERT_TRUE(boundary);
    output.push_back(*inside);
    output.push_back(*boundary);
    EXPECT_EQ(output.size(), 2);
}

TEST(EntityOverlay, ClampsAndValidatesHealthAndRadius) {
    using namespace ioj::ui::entity_overlay;
    Collector collector;
    collector.begin({}, 10.0f);
    auto low{collector.try_add({}, -0.5f, -1.0f, 0)};
    auto high{collector.try_add({}, 1.5f, 10.0f, 0)};
    auto invalid{collector.try_add(
        {}, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), 0)};
    ASSERT_TRUE(low && high && invalid);
    EXPECT_FLOAT_EQ(low->health, 0.0f);
    EXPECT_FLOAT_EQ(high->health, 1.0f);
    EXPECT_FLOAT_EQ(invalid->health, 0.0f);
    EXPECT_FLOAT_EQ(low->world_radius, 0.0f);
    EXPECT_FLOAT_EQ(high->world_radius, 10.0f);
    EXPECT_FLOAT_EQ(invalid->world_radius, 0.0f);
    EXPECT_EQ(collector.invalid_health_count(), 1);
}
