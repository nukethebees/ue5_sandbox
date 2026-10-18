#include <ioj/quantized_decimal.h>

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

struct DecimalCase {
    char const* name;
    double input;
    int integer;
    int fraction;
};
using DecimalConstruction = testing::TestWithParam<DecimalCase>;

TEST_P(DecimalConstruction, RoundsToCanonicalRepresentation) {
    auto const& test{GetParam()};
    TwoPlaceDecimal const value{test.input};
    EXPECT_EQ(value.integer(), test.integer);
    EXPECT_EQ(value.fraction(), test.fraction);
    EXPECT_NEAR(value.to_floating_point(), test.integer + test.fraction / 100.0, 1e-12);
}

constexpr DecimalCase decimal_cases[]{{"Zero", 0.0, 0, 0},
                                      {"Whole", 123.0, 123, 0},
                                      {"Fraction", 17.42, 17, 42},
                                      {"RoundsDown", 1.234, 1, 23},
                                      {"PositiveHalf", 1.125, 1, 13},
                                      {"NegativeHalf", -1.125, -2, 87},
                                      {"Carry", 1.999, 2, 0},
                                      {"NegativeWhole", -17.0, -17, 0},
                                      {"NegativeFraction", -17.42, -18, 58},
                                      {"NegativeSubunit", -0.42, -1, 58}};
INSTANTIATE_TEST_SUITE_P(NativeCoreQuantizedDecimal,
                         DecimalConstruction,
                         testing::ValuesIn(decimal_cases),
                         [](auto const& info) { return info.param.name; });

TEST(NativeCoreQuantizedDecimal, DefaultValueAndDecimalPlaceCounts) {
    EXPECT_EQ(TwoPlaceDecimal{}, TwoPlaceDecimal{0.0});
    EXPECT_EQ(WholeDecimal{17.6}.integer(), 18);
    EXPECT_EQ(WholeDecimal{17.6}.fraction(), 0);
    EXPECT_EQ(OnePlaceDecimal{17.42}.fraction(), 4);
    EXPECT_EQ(TwoPlaceDecimal{17.42}.fraction(), 42);
    EXPECT_EQ(ThreePlaceDecimal{17.425}.fraction(), 425);
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
