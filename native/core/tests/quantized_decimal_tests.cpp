#include <sandbox/core/quantized_decimal.h>

#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {
template <typename IntegerType, typename FractionType, std::size_t DecimalPlaces>
concept SupportsQuantizedDecimal =
    requires { typename ml::TQuantizedDecimal<IntegerType, FractionType, DecimalPlaces>; };

using CompactDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 2>;
using SmallDecimal = ml::TQuantizedDecimal<std::int8_t, std::uint8_t, 2>;
using OnePlaceDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 1>;
using TwoPlaceDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 2>;
using ThreePlaceDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint16_t, 3>;
using WholeDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 0>;

static_assert(SupportsQuantizedDecimal<std::int16_t, std::uint8_t, 2>);
static_assert(!SupportsQuantizedDecimal<std::uint16_t, std::uint8_t, 2>);
static_assert(!SupportsQuantizedDecimal<std::int16_t, std::int8_t, 2>);
static_assert(!SupportsQuantizedDecimal<std::int16_t, bool, 0>);
static_assert(!SupportsQuantizedDecimal<std::int16_t, std::uint8_t, 3>);
static_assert(!SupportsQuantizedDecimal<std::int64_t, std::uint64_t, 20>);
static_assert(std::same_as<CompactDecimal::integer_type, std::int16_t>);
static_assert(std::same_as<CompactDecimal::fraction_type, std::uint8_t>);
static_assert(CompactDecimal::decimal_places == 2);
static_assert(CompactDecimal::decimal_scale == 100);
static_assert(CompactDecimal::max_fraction == 99);
static_assert(std::is_trivially_copyable_v<CompactDecimal>);
static_assert(std::is_trivially_destructible_v<CompactDecimal>);
static_assert(std::is_standard_layout_v<CompactDecimal>);
static_assert(sizeof(CompactDecimal) == 4);
}

TEST(NativeCoreQuantizedDecimal, ConstructsZeroAndPositiveValues) {
    TwoPlaceDecimal const default_value{};
    TwoPlaceDecimal const zero{0.0};
    TwoPlaceDecimal const whole{123.0};
    TwoPlaceDecimal const fractional{17.42};

    EXPECT_EQ(default_value.integer(), 0);
    EXPECT_EQ(default_value.fraction(), 0);
    EXPECT_EQ(zero, default_value);
    EXPECT_EQ(whole.integer(), 123);
    EXPECT_EQ(whole.fraction(), 0);
    EXPECT_EQ(fractional.integer(), 17);
    EXPECT_EQ(fractional.fraction(), 42);
    EXPECT_NEAR(fractional.to_floating_point(), 17.42, 1e-12);
}

TEST(NativeCoreQuantizedDecimal, SupportsDifferentDecimalPlaceCounts) {
    WholeDecimal const whole{17.6};
    OnePlaceDecimal const one_place{17.42};
    TwoPlaceDecimal const two_places{17.42};
    ThreePlaceDecimal const three_places{17.425};

    EXPECT_EQ(whole.integer(), 18);
    EXPECT_EQ(whole.fraction(), 0);
    EXPECT_EQ(one_place.fraction(), 4);
    EXPECT_EQ(two_places.fraction(), 42);
    EXPECT_EQ(three_places.fraction(), 425);
}

TEST(NativeCoreQuantizedDecimal, RoundsAndCarries) {
    TwoPlaceDecimal const rounds_down{1.234};
    TwoPlaceDecimal const positive_half{1.125};
    TwoPlaceDecimal const negative_half{-1.125};
    TwoPlaceDecimal const carries{1.999};

    EXPECT_EQ(rounds_down.fraction(), 23);
    EXPECT_EQ(positive_half.fraction(), 13);
    EXPECT_EQ(negative_half.integer(), -2);
    EXPECT_EQ(negative_half.fraction(), 87);
    EXPECT_EQ(carries.integer(), 2);
    EXPECT_EQ(carries.fraction(), 0);
}

TEST(NativeCoreQuantizedDecimal, UsesCanonicalNegativeRepresentation) {
    TwoPlaceDecimal const whole{-17.0};
    TwoPlaceDecimal const fractional{-17.42};
    TwoPlaceDecimal const less_than_one{-0.42};

    EXPECT_EQ(whole.integer(), -17);
    EXPECT_EQ(whole.fraction(), 0);
    EXPECT_EQ(fractional.integer(), -18);
    EXPECT_EQ(fractional.fraction(), 58);
    EXPECT_NEAR(fractional.to_floating_point(), -17.42, 1e-12);
    EXPECT_EQ(less_than_one.integer(), -1);
    EXPECT_EQ(less_than_one.fraction(), 58);
}

TEST(NativeCoreQuantizedDecimal, SaturatesAndHandlesNonFiniteInput) {
    SmallDecimal const minimum{-128.0};
    SmallDecimal const maximum{127.99};
    SmallDecimal const below_minimum{-1000.0};
    SmallDecimal const above_maximum{1000.0};
    SmallDecimal const positive_infinity{std::numeric_limits<double>::infinity()};
    SmallDecimal const negative_infinity{-std::numeric_limits<double>::infinity()};
    SmallDecimal const nan{std::numeric_limits<double>::quiet_NaN()};

    EXPECT_EQ(below_minimum, minimum);
    EXPECT_EQ(above_maximum, maximum);
    EXPECT_EQ(positive_infinity, maximum);
    EXPECT_EQ(negative_infinity, minimum);
    EXPECT_EQ(nan, SmallDecimal{});
}

TEST(NativeCoreQuantizedDecimal, RoundTripsWithinQuantizationError) {
    constexpr double values[]{-127.994, -17.426, -0.125, 0.0, 0.125, 17.426, 127.984};
    constexpr double maximum_error{0.5 / TwoPlaceDecimal::decimal_scale + 1e-12};

    for (auto const value : values) {
        EXPECT_NEAR(TwoPlaceDecimal{value}.to_floating_point(), value, maximum_error);
    }
}

TEST(NativeCoreQuantizedDecimal, ProducesEveryOnePlaceFraction) {
    for (std::uint16_t fraction{}; fraction <= OnePlaceDecimal::max_fraction; ++fraction) {
        auto const input{12.0 + static_cast<double>(fraction) / OnePlaceDecimal::decimal_scale};
        OnePlaceDecimal const quantized{input};
        EXPECT_EQ(quantized.integer(), 12);
        EXPECT_EQ(quantized.fraction(), fraction);
    }
}
