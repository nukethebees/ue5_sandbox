#pragma once

#include "Math/UnrealMathUtility.h"

namespace ml::constants {
template <typename T>
inline constexpr T half_turn_degrees{T{180.0}};

template <typename T>
inline constexpr T full_turn_degrees{T{360.0}};

template <typename T>
inline constexpr T half_turn_radians{T{HALF_PI}};

template <typename T>
inline constexpr T full_turn_radians{T{PI}};
}
