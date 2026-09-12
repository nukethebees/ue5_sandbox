#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>

namespace ml::kernel {
template <typename T>
void assign_from(T* const dst_x,
                 T* const dst_y,
                 T* const dst_z,
                 T const* const src_x,
                 T const* const src_y,
                 T const* const src_z,
                 std::int32_t const count) {
    for (std::int32_t i{}; i < count; ++i) {
        dst_x[i] = src_x[i];
        dst_y[i] = src_y[i];
        dst_z[i] = src_z[i];
    }
}

template <typename T>
void fill(T* const values, T const value, std::int32_t const count) {
    for (std::int32_t i{}; i < count; ++i) {
        values[i] = value;
    }
}

template <typename T>
void fill(T* const xs, T* const ys, T* const zs, T const value, std::int32_t const count) {
    for (std::int32_t i{}; i < count; ++i) {
        xs[i] = value;
        ys[i] = value;
        zs[i] = value;
    }
}

template <std::floating_point T>
auto almost_equal(T const* const lhs,
                  T const* const rhs,
                  std::int32_t const count,
                  T const tolerance = static_cast<T>(1e-4)) -> bool {
    assert(lhs != nullptr);
    assert(rhs != nullptr);
    assert(count >= 0);

    for (std::int32_t i{}; i < count; ++i) {
        if (!(std::abs(lhs[i] - rhs[i]) <= tolerance)) {
            return false;
        }
    }

    return true;
}

auto is_sorted_desc(std::int32_t const* values, std::int32_t count) noexcept -> bool;
}
