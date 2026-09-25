#pragma once

#include "sandbox/core/vector_soa_view.h"

#include <cmath>
#include <cstdint>
#include <limits>

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
inline void solve_intercept_times(float* const out_intercept_times,
                                  auto const shooter_positions,
                                  auto const target_positions,
                                  auto const target_velocities,
                                  float const projectile_speed) noexcept {
    constexpr float no_intercept{};
    constexpr float epsilon{1e-8f};
    auto const projectile_speed_squared{projectile_speed * projectile_speed};
    constexpr float no_intercept_candidate_time{std::numeric_limits<float>::infinity()};
    auto const count{shooter_positions.num()};
    assert(count == target_positions.num() && count == target_velocities.num());
    assert(count == 0 || out_intercept_times != nullptr);

    auto const shooter_positions_xs{shooter_positions.xs()};
    auto const shooter_positions_ys{shooter_positions.ys()};
    auto const shooter_positions_zs{shooter_positions.zs()};
    auto const target_positions_xs{target_positions.xs()};
    auto const target_positions_ys{target_positions.ys()};
    auto const target_positions_zs{target_positions.zs()};
    auto const target_velocities_xs{target_velocities.xs()};
    auto const target_velocities_ys{target_velocities.ys()};
    auto const target_velocities_zs{target_velocities.zs()};

    for (std::int32_t i{}; i < count; ++i) {
        auto const rx{target_positions_xs[i] - shooter_positions_xs[i]};
        auto const ry{target_positions_ys[i] - shooter_positions_ys[i]};
        auto const rz{target_positions_zs[i] - shooter_positions_zs[i]};
        auto const velocity_x{target_velocities_xs[i]};
        auto const velocity_y{target_velocities_ys[i]};
        auto const velocity_z{target_velocities_zs[i]};
        auto const a{velocity_x * velocity_x + velocity_y * velocity_y + velocity_z * velocity_z -
                     projectile_speed_squared};
        auto const b{2.0f * (rx * velocity_x + ry * velocity_y + rz * velocity_z)};
        auto const c{rx * rx + ry * ry + rz * rz};

        out_intercept_times[i] = no_intercept;
        if (std::abs(a) < epsilon) {
            if (std::abs(b) >= epsilon) {
                auto const time{-c / b};
                if (time > 0.0f) {
                    out_intercept_times[i] = time;
                }
            }
            continue;
        }

        auto const discriminant{b * b - 4.0f * a * c};
        if (discriminant < 0.0f) {
            continue;
        }

        auto const scale{0.5f / a};
        auto const sqrt_discriminant{std::sqrt(discriminant)};
        auto const t0{scale * (-b - sqrt_discriminant)};
        auto const t1{scale * (-b + sqrt_discriminant)};
        auto best_time{no_intercept_candidate_time};
        if (t0 > 0.0f) {
            best_time = t0;
        }
        if (t1 > 0.0f && t1 < best_time) {
            best_time = t1;
        }
        if (best_time != no_intercept_candidate_time) {
            out_intercept_times[i] = best_time;
        }
    }
}
}
