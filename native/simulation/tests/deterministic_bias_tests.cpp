#include "sandbox/simulation/deterministic_bias.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <utility>

TEST(DeterministicBias, StableGeneration) {
    constexpr auto biases{ml::make_deterministic_biases(42, 7)};

    EXPECT_EQ(biases.integral, 2'408'474'220u);
    EXPECT_FLOAT_EQ(biases.floating, 0.85223466f);

    std::array const first_values{42, 43};
    std::array const second_values{7, 8};
    std::array<std::uint32_t, 2> integral_out{};
    std::array<float, 2> floating_out{};
    ASSERT_TRUE(
        ml::make_deterministic_biases(first_values, second_values, integral_out, floating_out));

    for (std::size_t i{0}; i < first_values.size(); ++i) {
        auto const expected{ml::make_deterministic_biases(first_values[i], second_values[i])};
        EXPECT_EQ(integral_out[i], expected.integral);
        EXPECT_FLOAT_EQ(floating_out[i], expected.floating);
    }
}

TEST(DeterministicBias, ValidFloatRange) {
    constexpr std::array cases{
        std::pair{0, 0},
        std::pair{1, 0},
        std::pair{0, 1},
        std::pair{-1, -1},
        std::pair{std::numeric_limits<std::int32_t>::min(),
                  std::numeric_limits<std::int32_t>::max()},
        std::pair{std::numeric_limits<std::int32_t>::max(),
                  std::numeric_limits<std::int32_t>::min()},
    };

    for (auto const [first, second] : cases) {
        auto const biases{ml::make_deterministic_biases(first, second)};
        EXPECT_GE(biases.floating, 0.f);
        EXPECT_LT(biases.floating, 1.f);
    }
}

TEST(DeterministicBias, RejectsMismatchedBatchSizes) {
    std::array const first{1, 2};
    std::array const second{3};
    std::array<std::uint32_t, 2> integral{};
    std::array<float, 2> floating{};

    EXPECT_FALSE(ml::make_deterministic_biases(first, second, integral, floating));
}
