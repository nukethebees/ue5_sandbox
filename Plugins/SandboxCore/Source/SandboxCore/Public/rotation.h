#pragma once

#include "numeric.h"

#include "CoreMinimal.h"

#include <algorithm>
#include <concepts>

namespace ml::constants {

template <typename T>
inline constexpr T zero{static_cast<T>(0.0)};

template <typename T>
inline constexpr T half_turn_degrees{static_cast<T>(180.0)};

template <typename T>
inline constexpr T full_turn_degrees{static_cast<T>(360.0)};

}

namespace ml::kernel {
template <std::floating_point T>
auto shortest_signed_angle_delta(T const from, T const to) -> T {
    constexpr auto ht{ml::constants::half_turn_degrees<T>};
    constexpr auto ft{ml::constants::full_turn_degrees<T>};
    constexpr auto zero{ml::constants::zero<T>};

    auto delta{FMath::Fmod(to - from + ht, ft)};

    if (delta < zero) {
        delta += ft;
    }

    return delta - ht;
}

template <Numeric T>
void rotate_towards_1d(T const* RESTRICT current,
                       T const* RESTRICT target,
                       T const speed,
                       T const delta_time,
                       T* RESTRICT out,
                       int32 const count) noexcept {}

#define ML_EXTERN_FN(T)                                                                  \
    extern template SANDBOXCORE_API void rotate_towards_1d<T>(T const* RESTRICT current, \
                                                              T const* RESTRICT target,  \
                                                              T const speed,             \
                                                              T const delta_time,        \
                                                              T* RESTRICT out,           \
                                                              int32 const count) noexcept
ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
