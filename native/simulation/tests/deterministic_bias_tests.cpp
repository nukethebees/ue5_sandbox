#include "ioj/sim/deterministic_bias.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <utility>

namespace ioj::sim::tests {

TEST(DeterministicBias, StableGeneration) {
    constexpr auto biases{make_deterministic_biases(42, 7)};

    EXPECT_EQ(biases.integral, 2'408'474'220u);
    EXPECT_FLOAT_EQ(biases.floating, 0.85223466f);

    std::array const first_values{42, 43};
    std::array const second_values{7, 8};
    std::array<std::uint32_t, 2> integral_out{};
    std::array<float, 2> floating_out{};
    ASSERT_TRUE(make_deterministic_biases(first_values, second_values, integral_out, floating_out));

    for (std::size_t i{0}; i < first_values.size(); ++i) {
        auto const expected{make_deterministic_biases(first_values[i], second_values[i])};
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
        auto const biases{make_deterministic_biases(first, second)};
        EXPECT_GE(biases.floating, 0.f);
        EXPECT_LT(biases.floating, 1.f);
    }
}

TEST(DeterministicBias, BothInputsAffectResults) {
    constexpr auto base{make_deterministic_biases(42, 7)};
    constexpr auto changed_first{make_deterministic_biases(43, 7)};
    constexpr auto changed_second{make_deterministic_biases(42, 8)};

    EXPECT_NE(changed_first.integral, base.integral);
    EXPECT_NE(changed_first.floating, base.floating);
    EXPECT_NE(changed_second.integral, base.integral);
    EXPECT_NE(changed_second.floating, base.floating);
}

TEST(DeterministicBias, AcceptsEmptyBatches) {
    std::array<std::int32_t, 0> const inputs{};
    std::array<std::uint32_t, 0> integral_out{};
    std::array<float, 0> floating_out{};

    EXPECT_TRUE(make_deterministic_biases(inputs, inputs, integral_out, floating_out));
}

TEST(DeterministicBias, GeneratesIntegralBiasesFromEntityHandles) {
    std::array const handles{RegistryEntityHandle{42, 7}, RegistryEntityHandle{43, 8}};
    std::array<std::uint32_t, 2> integral_out{};

    ASSERT_TRUE(make_deterministic_biases(handles, integral_out));
    EXPECT_EQ(integral_out[0], make_deterministic_integral_bias(42, 7));
    EXPECT_EQ(integral_out[1], make_deterministic_integral_bias(43, 8));
}

TEST(DeterministicBias, RejectsMismatchedBatchSizes) {
    std::array const first{1, 2};
    std::array const second{3};
    std::array<std::uint32_t, 2> integral{};
    std::array<float, 2> floating{};

    EXPECT_FALSE(make_deterministic_biases(first, second, integral, floating));
}

} // namespace tests
