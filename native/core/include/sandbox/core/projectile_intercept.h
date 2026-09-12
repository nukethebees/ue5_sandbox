#pragma once

#include <cstdint>

namespace ml {
struct NativeVector3f {
    float x{};
    float y{};
    float z{};
};

struct NativeVector3fSoAView {
    float const* xs{};
    float const* ys{};
    float const* zs{};
};

auto solve_intercept_time(NativeVector3f shooter_position,
                          NativeVector3f target_position,
                          NativeVector3f target_velocity,
                          float projectile_speed) noexcept -> float;
}

namespace ml::detail::solve_intercept_times_aos {
void solve_intercept_times(float* out_intercept_times,
                           NativeVector3fSoAView shooter_positions,
                           NativeVector3fSoAView target_positions,
                           NativeVector3fSoAView target_velocities,
                           float projectile_speed,
                           std::int32_t count) noexcept;
}

namespace ml::detail::solve_intercept_times_struct_loop {
void solve_intercept_times(float* out_intercept_times,
                           NativeVector3fSoAView shooter_positions,
                           NativeVector3fSoAView target_positions,
                           NativeVector3fSoAView target_velocities,
                           float projectile_speed,
                           std::int32_t count) noexcept;
}

namespace ml::detail::solve_intercept_times_soa_loop {
void solve_intercept_times(float* out_intercept_times,
                           NativeVector3fSoAView shooter_positions,
                           NativeVector3fSoAView target_positions,
                           NativeVector3fSoAView target_velocities,
                           float projectile_speed,
                           std::int32_t count) noexcept;
}
