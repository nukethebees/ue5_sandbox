#pragma once

#include "SandboxCore/public/numeric_constants.h"
#include "SandboxCore/public/array_utils.h"
#include "SandboxCore/public/numeric.h"

#include "CoreMinimal.h"

#include <algorithm>
#include <concepts>

namespace ml {
// Restrict angle to [-180, 180)
template <std::floating_point T>
auto normalise_degrees_signed_half_open_once(T const degrees) noexcept -> T {
    constexpr auto ht{ml::constants::half_turn_degrees<T>};
    constexpr auto ft{ml::constants::full_turn_degrees<T>};

    auto out{degrees};
    out -= ft * (out >= ht);
    out += ft * (out < -ht);

    return out;
}

template <std::floating_point T>
auto shortest_signed_angle_delta(T const from, T const to) -> T {
    constexpr auto ht{ml::constants::half_turn_degrees<T>};
    constexpr auto ft{ml::constants::full_turn_degrees<T>};

    auto delta{FMath::Fmod(to - from + ht, ft)};

    if (delta < T{0.0}) {
        delta += ft;
    }

    return delta - ht;
}

// A simpler version assuming values are within [-180.0, 180.0)
template <std::floating_point T>
auto shortest_signed_angle_delta_normalised(T const current, T const target) noexcept -> T {
    constexpr auto ht{ml::constants::half_turn_degrees<T>};
    constexpr auto ft{ml::constants::full_turn_degrees<T>};

    return normalise_degrees_signed_half_open_once(target - current);
}
}

namespace ml::kernel {
template <std::floating_point T>
void rotate_towards_1d_degrees(T const* RESTRICT current,
                               T const* RESTRICT target,
                               T const speed,
                               T const delta_time,
                               T* RESTRICT out,
                               int32 const count) noexcept {
    constexpr auto ft{ml::constants::full_turn_degrees<T>};

    auto const max_step{speed * delta_time};

    if (max_step <= T{0.0}) {
        for (int32 i{0}; i < count; ++i) {
            out[i] = current[i];
        }

        return;
    }

    for (int32 i{0}; i < count; ++i) {
        auto const delta{ml::shortest_signed_angle_delta(current[i], target[i])};
        auto const clamped_delta{FMath::Clamp(delta, -max_step, max_step)};
        auto const new_out{current[i] + clamped_delta};

        if (new_out > ft) {
            out[i] = new_out - ft;
        } else if (new_out < T{0.0}) {
            out[i] = new_out + ft;
        } else {
            out[i] = new_out;
        }
    }
}

// A simpler version assuming values are within [-180, 180)
template <std::floating_point T>
void rotate_towards_1d_degrees_normalised(T const* RESTRICT current,
                                          T const* RESTRICT target,
                                          T const speed,
                                          T const delta_time,
                                          T* RESTRICT out,
                                          int32 const count) noexcept {
    constexpr auto ht{ml::constants::half_turn_degrees<T>};
    constexpr auto ft{ml::constants::full_turn_degrees<T>};

    auto const max_step{speed * delta_time};

    if (max_step <= T{0.0}) {
        for (int32 i{0}; i < count; ++i) {
            out[i] = current[i];
        }

        return;
    }

    for (int32 i{0}; i < count; ++i) {
        auto const delta{ml::shortest_signed_angle_delta_normalised(current[i], target[i])};
        auto const clamped_delta{FMath::Clamp(delta, -max_step, max_step)};

        auto new_out{current[i] + clamped_delta};
        new_out = ml::normalise_degrees_signed_half_open_once(new_out);

        out[i] = new_out;
    }
}

template <std::floating_point T>
void rotate_towards_1d_degrees_normalised_inplace(T* RESTRICT current,
                                                  T const* RESTRICT target,
                                                  T const speed,
                                                  T const delta_time,
                                                  int32 const count) noexcept {
    constexpr auto ht{ml::constants::half_turn_degrees<T>};
    constexpr auto ft{ml::constants::full_turn_degrees<T>};

    auto const max_step{speed * delta_time};

    if (max_step <= T{0.0}) {
        return;
    }

    for (int32 i{0}; i < count; ++i) {
        auto const delta{ml::shortest_signed_angle_delta_normalised(current[i], target[i])};
        auto const clamped_delta{FMath::Clamp(delta, -max_step, max_step)};

        auto new_out{current[i] + clamped_delta};
        new_out = ml::normalise_degrees_signed_half_open_once(new_out);

        current[i] = new_out;
    }
}

template <std::floating_point T>
void compute_desired_yaws_radians(T const* RESTRICT const start_xs,
                                  T const* RESTRICT const start_ys,
                                  T const* RESTRICT const end_xs,
                                  T const* RESTRICT const end_ys,
                                  T* RESTRICT const out_yaws_radians,
                                  int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        auto const dx{end_xs[i] - start_xs[i]};
        auto const dy{end_ys[i] - start_ys[i]};

        out_yaws_radians[i] = FMath::Atan2(dy, dx);
    }
}
}

namespace ml {
template <std::floating_point T>
bool rotate_towards_1d_degrees_normalised_inplace(TArrayView<T> current,
                                                  TConstArrayView<T> target,
                                                  T const speed,
                                                  T const delta_time) noexcept {
    auto const count{current.Num()};

    if (!ml::detail::all_num_equal(count, target)) {
        return false;
    }

    ml::kernel::rotate_towards_1d_degrees_normalised_inplace(
        current.GetData(), target.GetData(), speed, delta_time, count);

    return true;
}
}
