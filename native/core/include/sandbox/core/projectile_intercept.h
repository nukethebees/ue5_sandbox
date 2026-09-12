#pragma once

#include "sandbox/core/math_types.h"

#include <cstdint>

namespace ml {
struct Vector3fSoAView {
    float const* xs{};
    float const* ys{};
    float const* zs{};
};

auto solve_intercept_time(Vector3f shooter_position,
                          Vector3f target_position,
                          Vector3f target_velocity,
                          float projectile_speed) noexcept -> float;
}

namespace ml::detail::solve_intercept_times_aos {
void solve_intercept_times(float* out_intercept_times,
                           Vector3fSoAView shooter_positions,
                           Vector3fSoAView target_positions,
                           Vector3fSoAView target_velocities,
                           float projectile_speed,
                           std::int32_t count) noexcept;
}

namespace ml::detail::solve_intercept_times_struct_loop {
void solve_intercept_times(float* out_intercept_times,
                           Vector3fSoAView shooter_positions,
                           Vector3fSoAView target_positions,
                           Vector3fSoAView target_velocities,
                           float projectile_speed,
                           std::int32_t count) noexcept;
}

namespace ml::detail::solve_intercept_times_soa_loop {
void solve_intercept_times(float* out_intercept_times,
                           Vector3fSoAView shooter_positions,
                           Vector3fSoAView target_positions,
                           Vector3fSoAView target_velocities,
                           float projectile_speed,
                           std::int32_t count) noexcept;
}
