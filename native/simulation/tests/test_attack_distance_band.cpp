#include <ioj/settings/attack_distance_band.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace ioj::sim::tests {
using settings::AttackDistanceBand;
TEST(AttackDistanceBand, FactoryPreservesValidDefaultsAndEndpoints) {
    AttackDistanceBand band;
    EXPECT_EQ(band.get().minimum, 0.4f);
    EXPECT_EQ(band.get().desired, 0.5f);
    EXPECT_EQ(band.get().maximum, 0.6f);

    for (auto const values : {std::array{0.f, 0.f, 0.f},
                              std::array{0.f, 0.f, 1.f},
                              std::array{0.f, 1.f, 1.f},
                              std::array{1.f, 1.f, 1.f}}) {
        SCOPED_TRACE(::testing::PrintToString(values));
        auto const result{
            settings::AttackDistanceBand::try_make({values[0], values[1], values[2]})};
        ASSERT_TRUE(result);
        band = *result;
        EXPECT_EQ(band.get().minimum, values[0]);
        EXPECT_EQ(band.get().desired, values[1]);
        EXPECT_EQ(band.get().maximum, values[2]);
    }
}

TEST(AttackDistanceBand, FactoryRejectsUnorderedOutOfRangeAndNonFiniteRatios) {
    auto const nan{std::numeric_limits<float>::quiet_NaN()};
    auto const infinity{std::numeric_limits<float>::infinity()};
    for (auto const values : {std::array{0.6f, 0.5f, 0.6f},
                              std::array{0.4f, 0.5f, 0.45f},
                              std::array{-0.1f, 0.5f, 0.6f},
                              std::array{0.4f, 0.5f, 1.1f},
                              std::array{nan, 0.5f, 0.6f},
                              std::array{0.4f, nan, 0.6f},
                              std::array{0.4f, 0.5f, nan},
                              std::array{-infinity, 0.5f, 0.6f},
                              std::array{0.4f, 0.5f, infinity}}) {
        SCOPED_TRACE(::testing::PrintToString(values));
        auto const result{
            settings::AttackDistanceBand::try_make({values[0], values[1], values[2]})};
        ASSERT_FALSE(result);
    }
}
}
