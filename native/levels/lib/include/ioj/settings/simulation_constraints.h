#pragma once

#include <ioj/constrained_value.h>
#include <ioj/sim/entities/health_type.h>

#include <cstdint>

namespace ioj::settings {
inline constexpr auto positive_health_policy{ConstrainedValuePolicy{
    [](sim::Health const value) constexpr noexcept { return value.value() > 0; },
    "Health must be positive"}};
inline constexpr auto dot_product_policy{ConstrainedValuePolicy{
    [](float const value) constexpr noexcept { return value >= -1.f && value <= 1.f; },
    "Dot product must be in [-1, 1]"}};
inline constexpr auto neighbour_threshold_policy{
    ConstrainedValuePolicy{[](std::int32_t const value) constexpr noexcept { return value >= 2; },
                           "Expected at least two neighbours"}};

using PositiveHealth = ConstrainedValue<sim::Health, positive_health_policy>;
using DotProduct = ConstrainedValue<float, dot_product_policy>;
using NeighbourThreshold = ConstrainedValue<std::int32_t, neighbour_threshold_policy>;
}
