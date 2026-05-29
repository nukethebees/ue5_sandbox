#pragma once

#include "SandboxCore/Public/angle_traits.h"
#include "SandboxCore/Public/angle_unit.h"
#include "SandboxCore/Public/array_utils.h"
#include "SandboxCore/Public/log_categories.h"
#include "SandboxCore/Public/numeric.h"
#include "SandboxCore/Public/numeric_constants.h"

#include "CoreMinimal.h"
#include "Containers/StringConv.h"

#include <algorithm>
#include <concepts>

namespace ml::detail {
// Restrict angle to [-ht, ht)
template <template <typename> typename AngleTraits, std::floating_point T>
auto normalise_signed_half_open_once(T const angle) noexcept -> T {
    constexpr auto ht{AngleTraits<T>::half_turn};
    constexpr auto ft{AngleTraits<T>::full_turn};

    auto out{angle};
    out -= ft * (out >= ht);
    out += ft * (out < -ht);

    return out;
}
template <template <typename> typename AngleTraits, std::floating_point T>
auto shortest_signed_angle_delta(T const from, T const to) -> T {
    constexpr auto ht{AngleTraits<T>::half_turn};
    constexpr auto ft{AngleTraits<T>::full_turn};

    auto delta{FMath::Fmod(to - from + ht, ft)};

    if (delta < T{0.0}) {
        delta += ft;
    }

    return delta - ht;
}
template <template <typename> typename AngleTraits, std::floating_point T>
auto shortest_signed_angle_delta_normalised(T const current, T const target) noexcept -> T {
    constexpr auto ht{AngleTraits<T>::half_turn};
    constexpr auto ft{AngleTraits<T>::full_turn};

    return normalise_signed_half_open_once<AngleTraits, T>(target - current);
}
} // namespace ml::detail

namespace ml {
template <std::floating_point T>
auto normalise_degrees_signed_half_open_once(T const angle) noexcept -> T {
    return ml::detail::normalise_signed_half_open_once<DegreesAngleTraits, T>(angle);
}
template <std::floating_point T>
auto normalise_radians_signed_half_open_once(T const angle) noexcept -> T {
    return ml::detail::normalise_signed_half_open_once<RadiansAngleTraits, T>(angle);
}

template <std::floating_point T>
auto shortest_signed_angle_delta_degrees(T const from, T const to) -> T {
    return ml::detail::shortest_signed_angle_delta<DegreesAngleTraits, T>(from, to);
}
template <std::floating_point T>
auto shortest_signed_angle_delta_radians(T const from, T const to) -> T {
    return ml::detail::shortest_signed_angle_delta<RadiansAngleTraits, T>(from, to);
}

// A simpler version assuming values are within [-180.0, 180.0)
template <std::floating_point T>
auto shortest_signed_angle_delta_degrees_normalised(T const current, T const target) noexcept -> T {
    return ml::detail::shortest_signed_angle_delta_normalised<DegreesAngleTraits, T>(current,
                                                                                     target);
}
template <std::floating_point T>
auto shortest_signed_angle_delta_radians_normalised(T const current, T const target) noexcept -> T {
    return ml::detail::shortest_signed_angle_delta_normalised<RadiansAngleTraits, T>(current,
                                                                                     target);
}
} // namespace ml

namespace ml::kernel::detail {
template <template <typename> typename AngleTraits, std::floating_point T>
void rotate_towards_1d(T const* RESTRICT current,
                       T const* RESTRICT target,
                       T const speed,
                       T const delta_time,
                       T* RESTRICT out,
                       int32 const count) noexcept {
    constexpr auto ft{AngleTraits<T>::full_turn};

    auto const max_step{speed * delta_time};

    if (max_step <= T{0.0}) {
        for (int32 i{0}; i < count; ++i) {
            out[i] = current[i];
        }

        return;
    }

    for (int32 i{0}; i < count; ++i) {
        auto const delta{
            ml::detail::shortest_signed_angle_delta<AngleTraits, T>(current[i], target[i])};
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

template <template <typename> typename AngleTraits, std::floating_point T>
void rotate_towards_1d_normalised(T const* RESTRICT current,
                                  T const* RESTRICT target,
                                  T const speed,
                                  T const delta_time,
                                  T* RESTRICT out,
                                  int32 const count) noexcept {
    constexpr auto ht{AngleTraits<T>::half_turn};
    constexpr auto ft{AngleTraits<T>::full_turn};

    auto const max_step{speed * delta_time};

    if (max_step <= T{0.0}) {
        for (int32 i{0}; i < count; ++i) {
            out[i] = current[i];
        }

        return;
    }

    for (int32 i{0}; i < count; ++i) {
        auto const delta{ml::detail::shortest_signed_angle_delta_normalised<AngleTraits, T>(
            current[i], target[i])};
        auto const clamped_delta{FMath::Clamp(delta, -max_step, max_step)};

        auto new_out{current[i] + clamped_delta};
        new_out = ml::detail::normalise_signed_half_open_once<AngleTraits, T>(new_out);

        out[i] = new_out;
    }
}

template <template <typename> typename AngleTraits, std::floating_point T>
void rotate_towards_1d_normalised_inplace(T* RESTRICT current,
                                          T const* RESTRICT target,
                                          T const speed,
                                          T const delta_time,
                                          int32 const count) noexcept {
    constexpr auto ht{AngleTraits<T>::half_turn};
    constexpr auto ft{AngleTraits<T>::full_turn};

    auto const max_step{speed * delta_time};

    if (max_step <= T{0.0}) {
        return;
    }

    for (int32 i{0}; i < count; ++i) {
        auto const delta{ml::detail::shortest_signed_angle_delta_normalised<AngleTraits, T>(
            current[i], target[i])};
        auto const clamped_delta{FMath::Clamp(delta, -max_step, max_step)};

        auto new_out{current[i] + clamped_delta};
        new_out = ml::detail::normalise_signed_half_open_once<AngleTraits, T>(new_out);

        current[i] = new_out;
    }
}
} // namespace ml::kernel::detail

namespace ml::kernel {
template <std::floating_point T>
void rotate_towards_1d_degrees(T const* RESTRICT current,
                               T const* RESTRICT target,
                               T const speed,
                               T const delta_time,
                               T* RESTRICT out,
                               int32 const count) noexcept {
    ml::kernel::detail::rotate_towards_1d<DegreesAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}
template <std::floating_point T>
void rotate_towards_1d_radians(T const* RESTRICT current,
                               T const* RESTRICT target,
                               T const speed,
                               T const delta_time,
                               T* RESTRICT out,
                               int32 const count) noexcept {
    ml::kernel::detail::rotate_towards_1d<RadiansAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}

// A simpler version assuming values are within [-180, 180)
template <std::floating_point T>
void rotate_towards_1d_degrees_normalised(T const* RESTRICT current,
                                          T const* RESTRICT target,
                                          T const speed,
                                          T const delta_time,
                                          T* RESTRICT out,
                                          int32 const count) noexcept {
    ml::kernel::detail::rotate_towards_1d_normalised<DegreesAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}
template <std::floating_point T>
void rotate_towards_1d_radians_normalised(T const* RESTRICT current,
                                          T const* RESTRICT target,
                                          T const speed,
                                          T const delta_time,
                                          T* RESTRICT out,
                                          int32 const count) noexcept {
    ml::kernel::detail::rotate_towards_1d_normalised<RadiansAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}

template <std::floating_point T>
void rotate_towards_1d_degrees_normalised_inplace(T* RESTRICT current,
                                                  T const* RESTRICT target,
                                                  T const speed,
                                                  T const delta_time,
                                                  int32 const count) noexcept {
    ml::kernel::detail::rotate_towards_1d_normalised_inplace<DegreesAngleTraits, T>(
        current, target, speed, delta_time, count);
}
template <std::floating_point T>
void rotate_towards_1d_radians_normalised_inplace(T* RESTRICT current,
                                                  T const* RESTRICT target,
                                                  T const speed,
                                                  T const delta_time,
                                                  int32 const count) noexcept {
    ml::kernel::detail::rotate_towards_1d_normalised_inplace<RadiansAngleTraits, T>(
        current, target, speed, delta_time, count);
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
} // namespace ml::kernel

namespace ml::detail {
template <template <typename> typename AngleTraits, std::floating_point T>
void rotate_towards_1d_normalised_inplace(TArrayView<T> current,
                                          TConstArrayView<T> target,
                                          T const speed,
                                          T const delta_time) noexcept {
    auto const count{current.Num()};

    auto const are_equal{ml::detail::all_num_equal(count, target)};
    check(are_equal);
    if (!are_equal) {
        UE_LOG(LogSandboxCore,
               Error,
               TEXT("rotate_towards_1d_normalised_inplace: Mismatched array sizes."));
        checkNoEntry();
        return;
    }

    ml::kernel::detail::rotate_towards_1d_normalised_inplace<AngleTraits, T>(
        current.GetData(), target.GetData(), speed, delta_time, count);
}
} // namespace ml::detail

namespace ml {
template <std::floating_point T>
void rotate_towards_1d_degrees_normalised_inplace(TArrayView<T> current,
                                                  TConstArrayView<T> target,
                                                  T const speed,
                                                  T const delta_time) noexcept {
    ml::detail::rotate_towards_1d_normalised_inplace<DegreesAngleTraits, T>(
        current, target, speed, delta_time);
}

template <std::floating_point T>
void rotate_towards_1d_radians_normalised_inplace(TArrayView<T> current,
                                                  TConstArrayView<T> target,
                                                  T const speed,
                                                  T const delta_time) noexcept {
    ml::detail::rotate_towards_1d_normalised_inplace<RadiansAngleTraits, T>(
        current, target, speed, delta_time);
}

template <std::floating_point T>
void compute_desired_yaws_radians(TConstArrayView<T> const start_xs,
                                  TConstArrayView<T> const start_ys,
                                  TConstArrayView<T> const end_xs,
                                  TConstArrayView<T> const end_ys,
                                  TArrayView<T> const out_yaws_radians) {
    auto const count{start_xs.Num()};

    auto const are_equal{
        ml::detail::all_num_equal(count, start_ys, end_xs, end_ys, out_yaws_radians)};
    check(are_equal);
    if (!are_equal) {
        UE_LOG(
            LogSandboxCore, Error, TEXT("compute_desired_yaws_radians: Mismatched array sizes."));
        checkNoEntry();
        return;
    }

    ml::kernel::compute_desired_yaws_radians<T>(start_xs.GetData(),
                                                start_ys.GetData(),
                                                end_xs.GetData(),
                                                end_ys.GetData(),
                                                out_yaws_radians.GetData(),
                                                count);
}
} // namespace ml
