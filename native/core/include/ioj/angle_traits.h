#pragma once

#include <ioj/numeric_constants.h>

#include <concepts>

namespace ioj {
template <std::floating_point T>
struct DegreesAngleTraits {
    static constexpr T half_turn{constants::half_turn_degrees<T>};
    static constexpr T full_turn{constants::full_turn_degrees<T>};
};

template <std::floating_point T>
struct RadiansAngleTraits {
    static constexpr T half_turn{constants::half_turn_radians<T>};
    static constexpr T full_turn{constants::full_turn_radians<T>};
};
}
