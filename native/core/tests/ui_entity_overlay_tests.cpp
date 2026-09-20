#include "sandbox/core/ui/entity_overlay.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace {
void apply(std::vector<ml::ui::entity_overlay::Instance>& output,
           ml::ui::entity_overlay::Addition addition) {
    output.push_back(addition.instance);
    if (addition.swap_index >= 0) {
        std::swap(output[static_cast<std::size_t>(addition.swap_index)], output.back());
    }
}
}

TEST(EntityOverlay, ValidatesSourceSizes) {
    using ml::ui::entity_overlay::source_sizes_match;
    EXPECT_TRUE(source_sizes_match(0, 0, 0));
    EXPECT_TRUE(source_sizes_match(2, 2, 2));
    EXPECT_FALSE(source_sizes_match(1, 0, 0));
}

TEST(EntityOverlay, PacksObjectiveAndClampedFillColor) {
    using namespace ml::ui::entity_overlay;
    auto const packed{pack_display_data(ObjectiveRole::Destroy, {-1.0f, 0.5f, 2.0f, 0.0f})};
    EXPECT_EQ(packed & 0x3U, static_cast<std::uint32_t>(ObjectiveRole::Destroy));
    EXPECT_NE(packed & (1U << 2U), 0U);
    EXPECT_EQ((packed >> 8U) & 0xffU, 0U);
    EXPECT_EQ((packed >> 16U) & 0xffU, 128U);
    EXPECT_EQ((packed >> 24U) & 0xffU, 255U);
}

TEST(EntityOverlay, FiltersRangeInclusivelyAndCompacts) {
    using namespace ml::ui::entity_overlay;
    Collector collector;
    collector.begin({}, 10.0f);
    std::vector<Instance> output;
    auto inside{collector.try_add({9.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 0)};
    auto boundary{collector.try_add({10.0f, 0.0f, 0.0f}, 0.5f, 1.0f, 0)};
    EXPECT_FALSE(collector.try_add({10.01f, 0.0f, 0.0f}, 1.0f, 1.0f, 0));
    ASSERT_TRUE(inside);
    ASSERT_TRUE(boundary);
    apply(output, *inside);
    apply(output, *boundary);
    EXPECT_EQ(output.size(), 2);
}

TEST(EntityOverlay, ClampsAndValidatesHealthAndRadius) {
    using namespace ml::ui::entity_overlay;
    Collector collector;
    collector.begin({}, 10.0f);
    auto low{collector.try_add({}, -0.5f, -1.0f, 0)};
    auto high{collector.try_add({}, 1.5f, 10.0f, 0)};
    auto invalid{collector.try_add(
        {}, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(), 0)};
    ASSERT_TRUE(low && high && invalid);
    EXPECT_FLOAT_EQ(low->instance.health, 0.0f);
    EXPECT_FLOAT_EQ(high->instance.health, 1.0f);
    EXPECT_FLOAT_EQ(invalid->instance.health, 0.0f);
    EXPECT_FLOAT_EQ(low->instance.world_radius, 0.0f);
    EXPECT_FLOAT_EQ(high->instance.world_radius, 10.0f);
    EXPECT_FLOAT_EQ(invalid->instance.world_radius, 0.0f);
    EXPECT_EQ(collector.invalid_health_count(), 1);
}

TEST(EntityOverlay, KeepsObjectivesAfterOrdinaryInstances) {
    using namespace ml::ui::entity_overlay;
    Collector collector;
    collector.begin({}, 100.0f);
    std::vector<Instance> output;
    apply(output, *collector.try_add({1.0f, 0.0f, 0.0f}, 0.25f, 10.0f, 0));
    apply(output,
          *collector.try_add(
              {2.0f, 0.0f, 0.0f}, 0.5f, 20.0f, pack_display_data(ObjectiveRole::Defend)));
    apply(output, *collector.try_add({3.0f, 0.0f, 0.0f}, 0.75f, 30.0f, 0));
    ASSERT_EQ(output.size(), 3);
    EXPECT_FLOAT_EQ(output[1].health, 0.75f);
    EXPECT_FLOAT_EQ(output[2].health, 0.5f);
    EXPECT_EQ(output[2].display_data & 0x3U, static_cast<std::uint32_t>(ObjectiveRole::Defend));
}
