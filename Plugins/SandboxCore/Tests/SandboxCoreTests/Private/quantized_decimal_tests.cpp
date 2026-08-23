#include <SandboxCore/quantized_decimal.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHarness.h"

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace {
template <typename IntegerType, typename FractionType, std::size_t DecimalPlaces>
concept SupportsQuantizedDecimal = requires { typename ml::TQuantizedDecimal<IntegerType, FractionType, DecimalPlaces>; };

using FCompactDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 2>;
using FSmallDecimal = ml::TQuantizedDecimal<std::int8_t, std::uint8_t, 2>;
using FOnePlaceDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 1>;
using FTwoPlaceDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 2>;
using FThreePlaceDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint16_t, 3>;
using FWholeDecimal = ml::TQuantizedDecimal<std::int16_t, std::uint8_t, 0>;
}

static_assert(SupportsQuantizedDecimal<std::int16_t, std::uint8_t, 2>);
static_assert(!SupportsQuantizedDecimal<std::uint16_t, std::uint8_t, 2>);
static_assert(!SupportsQuantizedDecimal<std::int16_t, std::int8_t, 2>);
static_assert(!SupportsQuantizedDecimal<std::int16_t, bool, 0>);
static_assert(!SupportsQuantizedDecimal<std::int16_t, std::uint8_t, 3>);
static_assert(!SupportsQuantizedDecimal<std::int64_t, std::uint64_t, 20>);
static_assert(std::same_as<FCompactDecimal::integer_type, std::int16_t>);
static_assert(std::same_as<FCompactDecimal::fraction_type, std::uint8_t>);
static_assert(FCompactDecimal::decimal_places == 2);
static_assert(FCompactDecimal::decimal_scale == 100);
static_assert(FCompactDecimal::max_fraction == 99);
static_assert(std::is_trivially_copyable_v<FCompactDecimal>);
static_assert(std::is_trivially_destructible_v<FCompactDecimal>);
static_assert(std::is_standard_layout_v<FCompactDecimal>);
static_assert(sizeof(FCompactDecimal) == 4);

TEST_CASE("SandboxCore.QuantizedDecimal.ConstructsZeroAndPositiveValues") {
    FTwoPlaceDecimal const default_value{};
    FTwoPlaceDecimal const zero{0.0};
    FTwoPlaceDecimal const whole{123.0};
    FTwoPlaceDecimal const fractional{17.42};

    CHECK(default_value.integer() == 0);
    CHECK(default_value.fraction() == 0);
    CHECK(zero == default_value);
    CHECK(whole.integer() == 123);
    CHECK(whole.fraction() == 0);
    CHECK(fractional.integer() == 17);
    CHECK(fractional.fraction() == 42);
    CHECK_THAT(fractional.to_floating_point(), Catch::Matchers::WithinAbs(17.42, 1e-12));
}

TEST_CASE("SandboxCore.QuantizedDecimal.SupportsDifferentDecimalPlaceCounts") {
    FWholeDecimal const whole{17.6};
    FOnePlaceDecimal const one_place{17.42};
    FTwoPlaceDecimal const two_places{17.42};
    FThreePlaceDecimal const three_places{17.425};

    CHECK(whole.integer() == 18);
    CHECK(whole.fraction() == 0);
    CHECK(one_place.integer() == 17);
    CHECK(one_place.fraction() == 4);
    CHECK(two_places.integer() == 17);
    CHECK(two_places.fraction() == 42);
    CHECK(three_places.integer() == 17);
    CHECK(three_places.fraction() == 425);
}

TEST_CASE("SandboxCore.QuantizedDecimal.RoundsToNearestAndCarries") {
    FTwoPlaceDecimal const rounds_down{1.234};
    FTwoPlaceDecimal const positive_half{1.125};
    FTwoPlaceDecimal const negative_half{-1.125};
    FTwoPlaceDecimal const carries{1.999};

    CHECK(rounds_down.integer() == 1);
    CHECK(rounds_down.fraction() == 23);
    CHECK(positive_half.integer() == 1);
    CHECK(positive_half.fraction() == 13);
    CHECK(negative_half.integer() == -2);
    CHECK(negative_half.fraction() == 87);
    CHECK(carries.integer() == 2);
    CHECK(carries.fraction() == 0);
}

TEST_CASE("SandboxCore.QuantizedDecimal.UsesCanonicalFloorRepresentationForNegatives") {
    FTwoPlaceDecimal const whole{-17.0};
    FTwoPlaceDecimal const fractional{-17.42};
    FTwoPlaceDecimal const less_than_one{-0.42};

    CHECK(whole.integer() == -17);
    CHECK(whole.fraction() == 0);
    CHECK(fractional.integer() == -18);
    CHECK(fractional.fraction() == 58);
    CHECK_THAT(fractional.to_floating_point(), Catch::Matchers::WithinAbs(-17.42, 1e-12));
    CHECK(less_than_one.integer() == -1);
    CHECK(less_than_one.fraction() == 58);
    CHECK_THAT(less_than_one.to_floating_point(), Catch::Matchers::WithinAbs(-0.42, 1e-12));
}

TEST_CASE("SandboxCore.QuantizedDecimal.SaturatesAtRepresentableBoundaries") {
    FSmallDecimal const minimum{-128.0};
    FSmallDecimal const maximum{127.99};
    FSmallDecimal const below_minimum{-1000.0};
    FSmallDecimal const above_maximum{1000.0};

    CHECK(minimum.integer() == std::numeric_limits<std::int8_t>::min());
    CHECK(minimum.fraction() == 0);
    CHECK(maximum.integer() == std::numeric_limits<std::int8_t>::max());
    CHECK(maximum.fraction() == FSmallDecimal::max_fraction);
    CHECK(below_minimum == minimum);
    CHECK(above_maximum == maximum);
}

TEST_CASE("SandboxCore.QuantizedDecimal.HandlesNonFiniteInputDeterministically") {
    FSmallDecimal const positive_infinity{std::numeric_limits<double>::infinity()};
    FSmallDecimal const negative_infinity{-std::numeric_limits<double>::infinity()};
    FSmallDecimal const nan{std::numeric_limits<double>::quiet_NaN()};

    CHECK(positive_infinity.integer() == std::numeric_limits<std::int8_t>::max());
    CHECK(positive_infinity.fraction() == FSmallDecimal::max_fraction);
    CHECK(negative_infinity.integer() == std::numeric_limits<std::int8_t>::min());
    CHECK(negative_infinity.fraction() == 0);
    CHECK(nan == FSmallDecimal{});
}

TEST_CASE("SandboxCore.QuantizedDecimal.RoundTripsWithinQuantizationError") {
    constexpr double values[]{-127.994, -17.426, -0.125, 0.0, 0.125, 17.426, 127.984};
    constexpr double maximum_error{0.5 / FTwoPlaceDecimal::decimal_scale};
    constexpr double comparison_epsilon{1e-12};

    for (auto const value : values) {
        FTwoPlaceDecimal const quantized{value};
        REQUIRE_THAT(quantized.to_floating_point(), Catch::Matchers::WithinAbs(value, maximum_error + comparison_epsilon));
    }
}

TEST_CASE("SandboxCore.QuantizedDecimal.ExhaustivelyProducesEveryOnePlaceFraction") {
    for (std::uint16_t fraction{}; fraction <= FOnePlaceDecimal::max_fraction; ++fraction) {
        auto const input{12.0 + static_cast<double>(fraction) / FOnePlaceDecimal::decimal_scale};
        FOnePlaceDecimal const quantized{input};

        CHECK(quantized.integer() == 12);
        CHECK(quantized.fraction() == fraction);
        CHECK(quantized.fraction() <= FOnePlaceDecimal::max_fraction);
    }
}
