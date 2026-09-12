#pragma once

#include <sandbox/core/angle_traits.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>

namespace ml::detail {
template <template <typename> typename AngleTraits, std::floating_point T>
auto normalise_signed_half_open_once(T const angle) noexcept -> T {
    constexpr auto half_turn{AngleTraits<T>::half_turn};
    constexpr auto full_turn{AngleTraits<T>::full_turn};

    auto out{angle};
    out -= full_turn * (out >= half_turn);
    out += full_turn * (out < -half_turn);

    return out;
}

template <template <typename> typename AngleTraits, std::floating_point T>
auto shortest_signed_angle_delta(T const from, T const to) -> T {
    constexpr auto half_turn{AngleTraits<T>::half_turn};
    constexpr auto full_turn{AngleTraits<T>::full_turn};

    auto delta{std::fmod(to - from + half_turn, full_turn)};
    if (delta < T{0.0}) {
        delta += full_turn;
    }

    return delta - half_turn;
}

template <template <typename> typename AngleTraits, std::floating_point T>
auto shortest_signed_angle_delta_normalised(T const current, T const target) noexcept -> T {
    return normalise_signed_half_open_once<AngleTraits, T>(target - current);
}
}

namespace ml {
template <std::floating_point T>
auto normalise_degrees_signed_half_open_once(T const angle) noexcept -> T {
    return detail::normalise_signed_half_open_once<DegreesAngleTraits, T>(angle);
}

template <std::floating_point T>
auto normalise_radians_signed_half_open_once(T const angle) noexcept -> T {
    return detail::normalise_signed_half_open_once<RadiansAngleTraits, T>(angle);
}

template <std::floating_point T>
auto shortest_signed_angle_delta_degrees(T const from, T const to) -> T {
    return detail::shortest_signed_angle_delta<DegreesAngleTraits, T>(from, to);
}

template <std::floating_point T>
auto shortest_signed_angle_delta_radians(T const from, T const to) -> T {
    return detail::shortest_signed_angle_delta<RadiansAngleTraits, T>(from, to);
}

template <std::floating_point T>
auto shortest_signed_angle_delta_degrees_normalised(T const current, T const target) noexcept -> T {
    return detail::shortest_signed_angle_delta_normalised<DegreesAngleTraits, T>(current, target);
}

template <std::floating_point T>
auto shortest_signed_angle_delta_radians_normalised(T const current, T const target) noexcept -> T {
    return detail::shortest_signed_angle_delta_normalised<RadiansAngleTraits, T>(current, target);
}
}

namespace ml::kernel::detail {
template <template <typename> typename AngleTraits, std::floating_point T>
void rotate_towards_1d(T const* const current,
                       T const* const target,
                       T const speed,
                       T const delta_time,
                       T* const out,
                       std::int32_t const count) noexcept {
    constexpr auto full_turn{AngleTraits<T>::full_turn};
    auto const max_step{speed * delta_time};

    if (max_step <= T{0.0}) {
        for (std::int32_t i{}; i < count; ++i) {
            out[i] = current[i];
        }
        return;
    }

    for (std::int32_t i{}; i < count; ++i) {
        auto const delta{
            ml::detail::shortest_signed_angle_delta<AngleTraits, T>(current[i], target[i])};
        auto const clamped_delta{std::clamp(delta, -max_step, max_step)};
        auto const new_out{current[i] + clamped_delta};

        if (new_out > full_turn) {
            out[i] = new_out - full_turn;
        } else if (new_out < T{0.0}) {
            out[i] = new_out + full_turn;
        } else {
            out[i] = new_out;
        }
    }
}

template <template <typename> typename AngleTraits, std::floating_point T>
void rotate_towards_1d_normalised(T const* const current,
                                  T const* const target,
                                  T const speed,
                                  T const delta_time,
                                  T* const out,
                                  std::int32_t const count) noexcept {
    auto const max_step{speed * delta_time};
    if (max_step <= T{0.0}) {
        for (std::int32_t i{}; i < count; ++i) {
            out[i] = current[i];
        }
        return;
    }

    for (std::int32_t i{}; i < count; ++i) {
        auto const delta{ml::detail::shortest_signed_angle_delta_normalised<AngleTraits, T>(
            current[i], target[i])};
        auto const clamped_delta{std::clamp(delta, -max_step, max_step)};
        out[i] =
            ml::detail::normalise_signed_half_open_once<AngleTraits, T>(current[i] + clamped_delta);
    }
}

template <template <typename> typename AngleTraits, std::floating_point T>
void rotate_towards_1d_normalised_in_place(T* const current,
                                           T const* const target,
                                           T const speed,
                                           T const delta_time,
                                           std::int32_t const count) noexcept {
    auto const max_step{speed * delta_time};
    if (max_step <= T{0.0}) {
        return;
    }

    for (std::int32_t i{}; i < count; ++i) {
        auto const delta{ml::detail::shortest_signed_angle_delta_normalised<AngleTraits, T>(
            current[i], target[i])};
        auto const clamped_delta{std::clamp(delta, -max_step, max_step)};
        current[i] =
            ml::detail::normalise_signed_half_open_once<AngleTraits, T>(current[i] + clamped_delta);
    }
}
}

namespace ml::kernel {
template <std::floating_point T>
void rotate_towards_1d_degrees(T const* const current,
                               T const* const target,
                               T const speed,
                               T const delta_time,
                               T* const out,
                               std::int32_t const count) noexcept {
    detail::rotate_towards_1d<DegreesAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}

template <std::floating_point T>
void rotate_towards_1d_radians(T const* const current,
                               T const* const target,
                               T const speed,
                               T const delta_time,
                               T* const out,
                               std::int32_t const count) noexcept {
    detail::rotate_towards_1d<RadiansAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}

template <std::floating_point T>
void rotate_towards_1d_degrees_normalised(T const* const current,
                                          T const* const target,
                                          T const speed,
                                          T const delta_time,
                                          T* const out,
                                          std::int32_t const count) noexcept {
    detail::rotate_towards_1d_normalised<DegreesAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}

template <std::floating_point T>
void rotate_towards_1d_radians_normalised(T const* const current,
                                          T const* const target,
                                          T const speed,
                                          T const delta_time,
                                          T* const out,
                                          std::int32_t const count) noexcept {
    detail::rotate_towards_1d_normalised<RadiansAngleTraits, T>(
        current, target, speed, delta_time, out, count);
}

template <std::floating_point T>
void rotate_towards_1d_degrees_normalised_in_place(T* const current,
                                                   T const* const target,
                                                   T const speed,
                                                   T const delta_time,
                                                   std::int32_t const count) noexcept {
    detail::rotate_towards_1d_normalised_in_place<DegreesAngleTraits, T>(
        current, target, speed, delta_time, count);
}

template <std::floating_point T>
void rotate_towards_1d_radians_normalised_in_place(T* const current,
                                                   T const* const target,
                                                   T const speed,
                                                   T const delta_time,
                                                   std::int32_t const count) noexcept {
    detail::rotate_towards_1d_normalised_in_place<RadiansAngleTraits, T>(
        current, target, speed, delta_time, count);
}

template <std::floating_point T>
void compute_desired_yaws_radians(T const* const start_xs,
                                  T const* const start_ys,
                                  T const* const end_xs,
                                  T const* const end_ys,
                                  T* const out_yaws_radians,
                                  std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        auto const dx{end_xs[i] - start_xs[i]};
        auto const dy{end_ys[i] - start_ys[i]};
        out_yaws_radians[i] = std::atan2(dy, dx);
    }
}

extern template void rotate_towards_1d_degrees<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
extern template void rotate_towards_1d_radians<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
extern template void rotate_towards_1d_degrees_normalised<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
extern template void rotate_towards_1d_radians_normalised<float>(
    float const*, float const*, float, float, float*, std::int32_t) noexcept;
extern template void rotate_towards_1d_degrees_normalised_in_place<float>(
    float*, float const*, float, float, std::int32_t) noexcept;
extern template void rotate_towards_1d_radians_normalised_in_place<float>(
    float*, float const*, float, float, std::int32_t) noexcept;
extern template void compute_desired_yaws_radians<float>(
    float const*, float const*, float const*, float const*, float*, std::int32_t) noexcept;
}
