#pragma once

#include "SandboxCore/array_checks.h"
#include "SandboxCore/log_categories.h"

#include <sandbox/core/rotation.h>

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"

#include <concepts>

namespace ml::detail {
template <std::floating_point T>
using RotateTowardsNormalisedInPlace = void (*)(T*, T const*, T, T, std::int32_t) noexcept;

template <std::floating_point T>
void rotate_towards_1d_normalised_in_place_adapter(
    TArrayView<T> const current,
    TConstArrayView<T> const target,
    T const speed,
    T const delta_time,
    RotateTowardsNormalisedInPlace<T> const rotate) noexcept {
    auto const count{current.Num()};
    auto const inputs_are_valid{ml::all_num_equal_and_pointers_not_equal(current, target)};
    check(inputs_are_valid);
    if (!inputs_are_valid) {
        UE_LOG(LogSandboxCore,
               Error,
               TEXT("rotate_towards_1d_normalised_in_place: Invalid array sizes or aliases."));
        checkNoEntry();
        return;
    }

    rotate(current.GetData(), target.GetData(), speed, delta_time, count);
}
}

namespace ml {
template <std::floating_point T>
void rotate_towards_1d_degrees_normalised_in_place(TArrayView<T> const current,
                                                   TConstArrayView<T> const target,
                                                   T const speed,
                                                   T const delta_time) noexcept {
    ml::detail::rotate_towards_1d_normalised_in_place_adapter(
        current,
        target,
        speed,
        delta_time,
        &ml::kernel::rotate_towards_1d_degrees_normalised_in_place<T>);
}

template <std::floating_point T>
void rotate_towards_1d_radians_normalised_in_place(TArrayView<T> const current,
                                                   TConstArrayView<T> const target,
                                                   T const speed,
                                                   T const delta_time) noexcept {
    ml::detail::rotate_towards_1d_normalised_in_place_adapter(
        current,
        target,
        speed,
        delta_time,
        &ml::kernel::rotate_towards_1d_radians_normalised_in_place<T>);
}

template <std::floating_point T>
void compute_desired_yaws_radians(TConstArrayView<T> const start_xs,
                                  TConstArrayView<T> const start_ys,
                                  TConstArrayView<T> const end_xs,
                                  TConstArrayView<T> const end_ys,
                                  TArrayView<T> const out_yaws_radians) {
    auto const count{start_xs.Num()};
    auto const inputs_are_valid{ml::all_num_equal_and_pointers_not_equal(
        start_xs, start_ys, end_xs, end_ys, out_yaws_radians)};
    check(inputs_are_valid);
    if (!inputs_are_valid) {
        UE_LOG(LogSandboxCore,
               Error,
               TEXT("compute_desired_yaws_radians: Invalid array sizes or aliases."));
        checkNoEntry();
        return;
    }

    ml::kernel::compute_desired_yaws_radians(start_xs.GetData(),
                                             start_ys.GetData(),
                                             end_xs.GetData(),
                                             end_ys.GetData(),
                                             out_yaws_radians.GetData(),
                                             count);
}
}
