#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <numbers>

namespace ml::native_math {
template <typename T>
auto size_squared(T const x, T const y, T const z) noexcept -> T {
    return x * x + y * y + z * z;
}

template <std::floating_point T>
auto size(T const x, T const y, T const z) noexcept -> T {
    return std::sqrt(size_squared(x, y, z));
}

template <typename T>
auto distance_squared(
    T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept -> T {
    return size_squared(bx - ax, by - ay, bz - az);
}

template <std::floating_point T>
auto distance(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept
    -> T {
    return std::sqrt(distance_squared(ax, ay, az, bx, by, bz));
}

template <typename T>
auto dot_product(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept
    -> T {
    return ax * bx + ay * by + az * bz;
}

template <typename T>
void add_vector3(T* out_x,
                 T* out_y,
                 T* out_z,
                 T const* lhs_x,
                 T const* lhs_y,
                 T const* lhs_z,
                 T const* rhs_x,
                 T const* rhs_y,
                 T const* rhs_z,
                 std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        out_x[i] = lhs_x[i] + rhs_x[i];
        out_y[i] = lhs_y[i] + rhs_y[i];
        out_z[i] = lhs_z[i] + rhs_z[i];
    }
}

template <typename T>
void add_vector3_in_place(T* dst_x,
                          T* dst_y,
                          T* dst_z,
                          T const* src_x,
                          T const* src_y,
                          T const* src_z,
                          std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        dst_x[i] += src_x[i];
        dst_y[i] += src_y[i];
        dst_z[i] += src_z[i];
    }
}

template <typename T>
void add_scaled_in_place(T* dst_x,
                         T* dst_y,
                         T* dst_z,
                         T const* src_x,
                         T const* src_y,
                         T const* src_z,
                         T const scale,
                         std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        dst_x[i] += src_x[i] * scale;
        dst_y[i] += src_y[i] * scale;
        dst_z[i] += src_z[i] * scale;
    }
}

template <typename T>
void add_scaled_product_in_place(T* dst_x,
                                 T* dst_y,
                                 T* dst_z,
                                 T const* a_x,
                                 T const* a_y,
                                 T const* a_z,
                                 T const* b_x,
                                 T const* b_y,
                                 T const* b_z,
                                 T const scale,
                                 std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        dst_x[i] += a_x[i] * b_x[i] * scale;
        dst_y[i] += a_y[i] * b_y[i] * scale;
        dst_z[i] += a_z[i] * b_z[i] * scale;
    }
}

template <typename T>
void add_scaled_product_in_place(T* dst_x,
                                 T* dst_y,
                                 T* dst_z,
                                 T const* a_x,
                                 T const* a_y,
                                 T const* a_z,
                                 T const* b,
                                 T const scale,
                                 std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        dst_x[i] += a_x[i] * b[i] * scale;
        dst_y[i] += a_y[i] * b[i] * scale;
        dst_z[i] += a_z[i] * b[i] * scale;
    }
}

template <typename T>
void subtract_scaled(T* out_x,
                     T* out_y,
                     T* out_z,
                     T const* a_x,
                     T const* a_y,
                     T const* a_z,
                     T const* b_x,
                     T const* b_y,
                     T const* b_z,
                     T const scale,
                     std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        out_x[i] = a_x[i] - b_x[i] * scale;
        out_y[i] = a_y[i] - b_y[i] * scale;
        out_z[i] = a_z[i] - b_z[i] * scale;
    }
}

template <typename T>
void subtract_scaled_in_place(T* a_x,
                              T* a_y,
                              T* a_z,
                              T const* b_x,
                              T const* b_y,
                              T const* b_z,
                              T const scale,
                              std::int32_t const count) noexcept {
    subtract_scaled(a_x, a_y, a_z, a_x, a_y, a_z, b_x, b_y, b_z, scale, count);
}

template <typename T, typename Scale>
void multiply_vector3(T* dst_x,
                      T* dst_y,
                      T* dst_z,
                      T const* src_x,
                      T const* src_y,
                      T const* src_z,
                      Scale const scale,
                      std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        auto const value{[&] {
            if constexpr (requires { scale[i]; }) {
                return scale[i];
            } else {
                return scale;
            }
        }()};
        dst_x[i] = src_x[i] * value;
        dst_y[i] = src_y[i] * value;
        dst_z[i] = src_z[i] * value;
    }
}

template <typename T>
void multiply_vector3_in_place(T* x,
                               T* y,
                               T* z,
                               T const* scale_x,
                               T const* scale_y,
                               T const* scale_z,
                               std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        x[i] *= scale_x[i];
        y[i] *= scale_y[i];
        z[i] *= scale_z[i];
    }
}

template <typename T>
void size_squared_vector(
    T* out, T const* x, T const* y, T const* z, std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        out[i] = size_squared(x[i], y[i], z[i]);
    }
}

template <typename T>
void distance_squared_vector(T* out,
                             T const* ax,
                             T const* ay,
                             T const* az,
                             T const* bx,
                             T const* by,
                             T const* bz,
                             std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        out[i] = distance_squared(ax[i], ay[i], az[i], bx[i], by[i], bz[i]);
    }
}

template <std::floating_point T>
void distance_and_squared_vector(T* out_distance,
                                 T* out_squared,
                                 T const* ax,
                                 T const* ay,
                                 T const* az,
                                 T const* bx,
                                 T const* by,
                                 T const* bz,
                                 std::int32_t const count) noexcept {
    distance_squared_vector(out_squared, ax, ay, az, bx, by, bz, count);
    for (std::int32_t i{}; i < count; ++i) {
        out_distance[i] = std::sqrt(out_squared[i]);
    }
}

template <typename T>
void dot_product_vector(T* out,
                        T const* ax,
                        T const* ay,
                        T const* az,
                        T const* bx,
                        T const* by,
                        T const* bz,
                        std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        out[i] = dot_product(ax[i], ay[i], az[i], bx[i], by[i], bz[i]);
    }
}

template <std::floating_point T>
void direction_and_distance(T* out_x,
                            T* out_y,
                            T* out_z,
                            T* out_distance,
                            T const* from_x,
                            T const* from_y,
                            T const* from_z,
                            T const* to_x,
                            T const* to_y,
                            T const* to_z,
                            std::int32_t const count) noexcept {
    constexpr auto small_number{static_cast<T>(1.0e-8)};
    for (std::int32_t i{}; i < count; ++i) {
        auto const dx{to_x[i] - from_x[i]};
        auto const dy{to_y[i] - from_y[i]};
        auto const dz{to_z[i] - from_z[i]};
        auto const squared{size_squared(dx, dy, dz)};
        if (squared <= small_number) {
            out_x[i] = T{};
            out_y[i] = T{};
            out_z[i] = T{};
            if (out_distance) {
                out_distance[i] = T{};
            }
            continue;
        }
        auto const distance_value{std::sqrt(squared)};
        auto const inverse{T{1} / distance_value};
        out_x[i] = dx * inverse;
        out_y[i] = dy * inverse;
        out_z[i] = dz * inverse;
        if (out_distance) {
            out_distance[i] = distance_value;
        }
    }
}

template <std::floating_point T>
void to_rotations(T* pitches,
                  T* yaws,
                  T* rolls,
                  T const* xs,
                  T const* ys,
                  T const* zs,
                  std::int32_t const count) noexcept {
    constexpr auto radians_to_degrees{T{180} / std::numbers::pi_v<T>};
    for (std::int32_t i{}; i < count; ++i) {
        yaws[i] = std::atan2(ys[i], xs[i]) * radians_to_degrees;
        pitches[i] =
            std::atan2(zs[i], std::sqrt(xs[i] * xs[i] + ys[i] * ys[i])) * radians_to_degrees;
        rolls[i] = T{};
    }
}
}
