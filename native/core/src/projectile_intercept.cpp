#include <sandbox/core/projectile_intercept.h>

#include <cmath>
#include <limits>

namespace {
auto size_squared(ml::NativeVector3f const value) noexcept -> float {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

auto dot_product(ml::NativeVector3f const lhs, ml::NativeVector3f const rhs) noexcept -> float {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

auto at(ml::NativeVector3fSoAView const values, std::int32_t const index) noexcept
    -> ml::NativeVector3f {
    return {values.xs[index], values.ys[index], values.zs[index]};
}
}

namespace ml {
auto solve_intercept_time(NativeVector3f const shooter_position,
                          NativeVector3f const target_position,
                          NativeVector3f const target_velocity,
                          float const projectile_speed) noexcept -> float {
    constexpr float no_intercept{};
    constexpr float epsilon{1e-8f};

    auto const relative_position{NativeVector3f{
        target_position.x - shooter_position.x,
        target_position.y - shooter_position.y,
        target_position.z - shooter_position.z,
    }};
    auto const a{size_squared(target_velocity) - projectile_speed * projectile_speed};
    auto const b{2.0f * dot_product(relative_position, target_velocity)};
    auto const c{size_squared(relative_position)};

    if (std::abs(a) < epsilon) {
        if (std::abs(b) < epsilon) {
            return no_intercept;
        }

        auto const time{-c / b};
        return time > 0.0f ? time : no_intercept;
    }

    auto const discriminant{b * b - 4.0f * a * c};
    if (discriminant < 0.0f) {
        return no_intercept;
    }

    auto const sqrt_discriminant{std::sqrt(discriminant)};
    auto const t0{(-b - sqrt_discriminant) / (2.0f * a)};
    auto const t1{(-b + sqrt_discriminant) / (2.0f * a)};
    auto best_time{std::numeric_limits<float>::max()};

    if (t0 > 0.0f) {
        best_time = t0;
    }
    if (t1 > 0.0f && t1 < best_time) {
        best_time = t1;
    }

    return best_time == std::numeric_limits<float>::max() ? no_intercept : best_time;
}
}

namespace ml::detail::solve_intercept_times_aos {
void solve_intercept_times(float* const out_intercept_times,
                           NativeVector3fSoAView const shooter_positions,
                           NativeVector3fSoAView const target_positions,
                           NativeVector3fSoAView const target_velocities,
                           float const projectile_speed,
                           std::int32_t const count) noexcept {
    for (std::int32_t i{}; i < count; ++i) {
        out_intercept_times[i] = ml::solve_intercept_time(at(shooter_positions, i),
                                                          at(target_positions, i),
                                                          at(target_velocities, i),
                                                          projectile_speed);
    }
}
}

namespace ml::detail::solve_intercept_times_struct_loop {
void solve_intercept_times(float* const out_intercept_times,
                           NativeVector3fSoAView const shooter_positions,
                           NativeVector3fSoAView const target_positions,
                           NativeVector3fSoAView const target_velocities,
                           float const projectile_speed,
                           std::int32_t const count) noexcept {
    constexpr float no_intercept{};
    constexpr float epsilon{1e-8f};
    constexpr auto maximum{std::numeric_limits<float>::max()};
    auto const projectile_speed_squared{projectile_speed * projectile_speed};

    for (std::int32_t i{}; i < count; ++i) {
        auto const shooter_position{at(shooter_positions, i)};
        auto const target_position{at(target_positions, i)};
        auto const target_velocity{at(target_velocities, i)};
        auto const relative_position{NativeVector3f{
            target_position.x - shooter_position.x,
            target_position.y - shooter_position.y,
            target_position.z - shooter_position.z,
        }};
        auto const a{size_squared(target_velocity) - projectile_speed_squared};
        auto const b{2.0f * dot_product(relative_position, target_velocity)};
        auto const c{size_squared(relative_position)};

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
        auto best_time{maximum};
        if (t0 > 0.0f) {
            best_time = t0;
        }
        if (t1 > 0.0f && t1 < best_time) {
            best_time = t1;
        }
        if (best_time != maximum) {
            out_intercept_times[i] = best_time;
        }
    }
}
}

namespace ml::detail::solve_intercept_times_soa_loop {
void solve_intercept_times(float* const out_intercept_times,
                           NativeVector3fSoAView const shooter_positions,
                           NativeVector3fSoAView const target_positions,
                           NativeVector3fSoAView const target_velocities,
                           float const projectile_speed,
                           std::int32_t const count) noexcept {
    constexpr float no_intercept{};
    constexpr float epsilon{1e-8f};
    constexpr auto maximum{std::numeric_limits<float>::max()};
    auto const projectile_speed_squared{projectile_speed * projectile_speed};

    for (std::int32_t i{}; i < count; ++i) {
        auto const rx{target_positions.xs[i] - shooter_positions.xs[i]};
        auto const ry{target_positions.ys[i] - shooter_positions.ys[i]};
        auto const rz{target_positions.zs[i] - shooter_positions.zs[i]};
        auto const velocity_x{target_velocities.xs[i]};
        auto const velocity_y{target_velocities.ys[i]};
        auto const velocity_z{target_velocities.zs[i]};
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
        auto best_time{maximum};
        if (t0 > 0.0f) {
            best_time = t0;
        }
        if (t1 > 0.0f && t1 < best_time) {
            best_time = t1;
        }
        if (best_time != maximum) {
            out_intercept_times[i] = best_time;
        }
    }
}
}
