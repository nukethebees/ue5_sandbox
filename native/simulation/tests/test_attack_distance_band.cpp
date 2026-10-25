#include <ioj/settings/attack_distance_band.h>
#include <ioj/testing/structural_diff.h>

#include <gtest/gtest.h>

#include <array>
#include <format>
#include <limits>

namespace ioj::sim::tests {
using ioj::testing::structurally_equal;
using settings::AttackDistanceBand;
using settings::AttackDistanceBandValue;

TEST(AttackDistanceBand, FactoryPreservesValidDefaultsAndEndpoints) {
    AttackDistanceBand band;
    AttackDistanceBandValue const defaults{0.4f, 0.5f, 0.6f};
    EXPECT_PRED_FORMAT2(structurally_equal, defaults, band.get());

    std::array<AttackDistanceBandValue, 4> const cases{
        {{0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 1.f}, {1.f, 1.f, 1.f}}};
    for (auto const values : cases) {
        SCOPED_TRACE(std::format("({}, {}, {})", values.minimum, values.desired, values.maximum));
        auto const result{AttackDistanceBand::try_make(values)};
        ASSERT_TRUE(result);
        band = *result;
        EXPECT_PRED_FORMAT2(structurally_equal, values, band.get());
    }
}

TEST(AttackDistanceBand, FactoryRejectsUnorderedOutOfRangeAndNonFiniteRatios) {
    auto const nan{std::numeric_limits<float>::quiet_NaN()};
    auto const infinity{std::numeric_limits<float>::infinity()};
    std::array<AttackDistanceBandValue, 9> const cases{{{0.6f, 0.5f, 0.6f},
                                                        {0.4f, 0.5f, 0.45f},
                                                        {-0.1f, 0.5f, 0.6f},
                                                        {0.4f, 0.5f, 1.1f},
                                                        {nan, 0.5f, 0.6f},
                                                        {0.4f, nan, 0.6f},
                                                        {0.4f, 0.5f, nan},
                                                        {-infinity, 0.5f, 0.6f},
                                                        {0.4f, 0.5f, infinity}}};
    for (auto const values : cases) {
        SCOPED_TRACE(std::format("({}, {}, {})", values.minimum, values.desired, values.maximum));
        auto const result{AttackDistanceBand::try_make(values)};
        ASSERT_FALSE(result);
    }
}
}
