#pragma once

#include <numbers>

namespace ml::constants {
template <typename T>
inline constexpr T half_turn_degrees{T{180.0}};

template <typename T>
inline constexpr T full_turn_degrees{T{360.0}};

template <typename T>
inline constexpr T half_turn_radians{std::numbers::pi_v<T> / T{2.0}};

template <typename T>
inline constexpr T full_turn_radians{std::numbers::pi_v<T>};
}
