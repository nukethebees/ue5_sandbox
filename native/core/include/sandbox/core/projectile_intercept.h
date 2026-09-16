#pragma once

#include "sandbox/core/vector_soa_view.h"

#include <cstdint>

namespace ml {
auto solve_intercept_time(Vector3f shooter_position,
                          Vector3f target_position,
                          Vector3f target_velocity,
                          float projectile_speed) noexcept -> float;
}

namespace ml::detail::solve_intercept_times_aos {
void solve_intercept_times(float* out_intercept_times,
                           Vector3fSoAConstView shooter_positions,
                           Vector3fSoAConstView target_positions,
                           Vector3fSoAConstView target_velocities,
                           float projectile_speed) noexcept;
}

namespace ml::detail::solve_intercept_times_struct_loop {
void solve_intercept_times(float* out_intercept_times,
                           Vector3fSoAConstView shooter_positions,
                           Vector3fSoAConstView target_positions,
                           Vector3fSoAConstView target_velocities,
                           float projectile_speed) noexcept;
}

namespace ml::detail::solve_intercept_times_soa_loop {
void solve_intercept_times(float* out_intercept_times,
                           Vector3fSoAConstView shooter_positions,
                           Vector3fSoAConstView target_positions,
                           Vector3fSoAConstView target_velocities,
                           float projectile_speed) noexcept;
}
