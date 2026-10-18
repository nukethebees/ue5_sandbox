#include <ioj/projectile_intercept.h>

#include <cmath>
#include <limits>

namespace {
constexpr float no_intercept_candidate_time{std::numeric_limits<float>::infinity()};

auto size_squared(ioj::Vector3f const value) noexcept -> float {
    return ioj::size_squared(value);
}

auto dot_product(ioj::Vector3f const lhs, ioj::Vector3f const rhs) noexcept -> float {
    return ioj::dot(lhs, rhs);
}

auto at(ioj::Vector3fSoAConstView const values, std::uint32_t const index) noexcept -> ioj::Vector3f {
    return values[index];
}

auto validate_batch([[maybe_unused]] float const* const out_intercept_times,
                    ioj::Vector3fSoAConstView const shooter_positions,
                    [[maybe_unused]] ioj::Vector3fSoAConstView const target_positions,
                    [[maybe_unused]] ioj::Vector3fSoAConstView const target_velocities) noexcept
    -> std::uint32_t {
    auto const count{shooter_positions.num()};
    assert(count == target_positions.num());
    assert(count == target_velocities.num());
    assert(count == 0 || out_intercept_times != nullptr);
    return count;
}
}

namespace ioj {
auto solve_intercept_time(Vector3f const shooter_position,
                          Vector3f const target_position,
                          Vector3f const target_velocity,
                          float const projectile_speed) noexcept -> float {
    constexpr float no_intercept{};
    constexpr float epsilon{1e-8f};

    auto const relative_position{target_position - shooter_position};
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
    auto best_time{no_intercept_candidate_time};

    if (t0 > 0.0f) {
        best_time = t0;
    }
    if (t1 > 0.0f && t1 < best_time) {
        best_time = t1;
    }

    return best_time == no_intercept_candidate_time ? no_intercept : best_time;
}
}

namespace ioj::detail::solve_intercept_times_aos {
void solve_intercept_times(float* const out_intercept_times,
                           Vector3fSoAConstView const shooter_positions,
                           Vector3fSoAConstView const target_positions,
                           Vector3fSoAConstView const target_velocities,
                           float const projectile_speed) noexcept {
    auto const count{validate_batch(
        out_intercept_times, shooter_positions, target_positions, target_velocities)};
    for (std::uint32_t i{}; i < count; ++i) {
        out_intercept_times[i] = ioj::solve_intercept_time(at(shooter_positions, i),
                                                          at(target_positions, i),
                                                          at(target_velocities, i),
                                                          projectile_speed);
    }
}
}

namespace ioj::detail::solve_intercept_times_struct_loop {
void solve_intercept_times(float* const out_intercept_times,
                           Vector3fSoAConstView const shooter_positions,
                           Vector3fSoAConstView const target_positions,
                           Vector3fSoAConstView const target_velocities,
                           float const projectile_speed) noexcept {
    constexpr float no_intercept{};
    constexpr float epsilon{1e-8f};
    auto const projectile_speed_squared{projectile_speed * projectile_speed};
    auto const count{validate_batch(
        out_intercept_times, shooter_positions, target_positions, target_velocities)};

    for (std::uint32_t i{}; i < count; ++i) {
        auto const shooter_position{at(shooter_positions, i)};
        auto const target_position{at(target_positions, i)};
        auto const target_velocity{at(target_velocities, i)};
        auto const relative_position{target_position - shooter_position};
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
