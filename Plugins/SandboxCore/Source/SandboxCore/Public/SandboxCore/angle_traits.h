#pragma once

#include <SandboxCore/numeric_constants.h>

#include <concepts>

namespace ml {
template <std::floating_point T>
struct DegreesAngleTraits {
    static constexpr T half_turn{ml::constants::half_turn_degrees<T>};
    static constexpr T full_turn{ml::constants::full_turn_degrees<T>};
};

template <std::floating_point T>
struct RadiansAngleTraits {
    static constexpr T half_turn{ml::constants::half_turn_radians<T>};
    static constexpr T full_turn{ml::constants::full_turn_radians<T>};
};
}
