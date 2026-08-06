#include <SandboxCore/array_checks.h>
#include <SandboxCore/projectile_intercept.h>
#include <SandboxCore/soa_vector_utils.h>
#include <SandboxCore/soa_vectors.h>

namespace ml {
auto solve_intercept_time(FVector3f const& shooter_pos,
                          FVector3f const& target_pos,
                          FVector3f const& target_vel,
                          float const projectile_speed) -> float {
    using Vec3 = FVector3f;
    using value_type = float;

    value_type constexpr no_intercept{};
    value_type constexpr eps{1e-8f};

    Vec3 const r{target_pos - shooter_pos};

    value_type const a{static_cast<value_type>(target_vel.SizeSquared()) -
                       projectile_speed * projectile_speed};
    value_type const b{2.0f * static_cast<value_type>(Vec3::DotProduct(r, target_vel))};
    value_type const c{static_cast<value_type>(r.SizeSquared())};

    if (FMath::Abs(a) < eps) {
        if (FMath::Abs(b) < eps) {
            return no_intercept;
        }

        value_type const t{-c / b};
        if (t > 0.f) {
            return t;
        }

        return no_intercept;
    }

    value_type const discriminant{b * b - 4.0f * a * c};
    if (discriminant < 0.f) {
        return no_intercept;
    }

    value_type const sqrt_discriminant{FMath::Sqrt(discriminant)};

    value_type const t0{(-b - sqrt_discriminant) / (2.0f * a)};
    value_type const t1{(-b + sqrt_discriminant) / (2.0f * a)};

    value_type best_t{TNumericLimits<value_type>::Max()};

    if (t0 > 0.f) {
        best_t = t0;
    }

    if ((t1 > 0.f) && (t1 < best_t)) {
        best_t = t1;
    }

    if (best_t == TNumericLimits<value_type>::Max()) {
        return {};
    }

    return best_t;
}
}

namespace ml::detail::solve_intercept_times_aos {
void solve_intercept_times(TArrayView<float> const out_intercept_times,
                           FVectors3f::ConstView const shooter_positions,
                           FVectors3f::ConstView const target_positions,
                           FVectors3f::ConstView const target_velocities,
                           float const projectile_speed) {
    auto const count{out_intercept_times.Num()};

    check(ml::all_num_equal_and_pointers_not_equal(
        out_intercept_times, shooter_positions, target_positions, target_velocities));

    for (int32 i{0}; i < count; ++i) {
        auto const shooter_pos{
            FVector3f{shooter_positions.xs[i], shooter_positions.ys[i], shooter_positions.zs[i]}};
        auto const target_pos{
            FVector3f{target_positions.xs[i], target_positions.ys[i], target_positions.zs[i]}};
        auto const target_vel{
            FVector3f{target_velocities.xs[i], target_velocities.ys[i], target_velocities.zs[i]}};

        out_intercept_times[i] =
            solve_intercept_time(shooter_pos, target_pos, target_vel, projectile_speed);
    }
}
}

namespace ml::detail::solve_intercept_times_struct_loop {
void solve_intercept_times(TArrayView<float> const out_intercept_times,
                           FVectors3f::ConstView const shooter_positions,
                           FVectors3f::ConstView const target_positions,
                           FVectors3f::ConstView const target_velocities,
                           float const projectile_speed) {
    using Vec3 = FVector3f;
    using value_type = float;

    value_type constexpr no_intercept{};
    value_type constexpr eps{1e-8f};

    auto const count{out_intercept_times.Num()};

    check(ml::all_num_equal_and_pointers_not_equal(
        out_intercept_times, shooter_positions, target_positions, target_velocities));

    auto const projectile_speed_sq{projectile_speed * projectile_speed};
    constexpr value_type max{TNumericLimits<value_type>::Max()};

    for (int32 i{0}; i < count; ++i) {
        auto const target_pos{get_vector3f(target_positions, i)};
        auto const shooter_pos{get_vector3f(shooter_positions, i)};
        auto const target_vel{get_vector3f(target_velocities, i)};

        Vec3 const r{target_pos - shooter_pos};

        value_type const a{target_vel.SizeSquared() - projectile_speed_sq};
        value_type const b{2.0f * Vec3::DotProduct(r, target_vel)};
        value_type const c{r.SizeSquared()};

        out_intercept_times[i] = no_intercept;

        if (FMath::Abs(a) < eps) {
            if (FMath::Abs(b) < eps) {
                continue;
            }

            value_type const t{-c / b};
            if (t > 0.f) {
                out_intercept_times[i] = t;
                continue;
            }

            continue;
        }

        value_type const discriminant{b * b - 4.0f * a * c};
        if (discriminant < 0.f) {
            continue;
        }

        value_type const sqrt_discriminant{FMath::Sqrt(discriminant)};

        value_type const k{0.5f / a};

        value_type const t0{k * (-b - sqrt_discriminant)};
        value_type const t1{k * (-b + sqrt_discriminant)};

        value_type best_t{max};

        if (t0 > 0.f) {
            best_t = t0;
        }

        if ((t1 > 0.f) && (t1 < best_t)) {
            best_t = t1;
        }

        if (best_t == max) {
            continue;
        }

        out_intercept_times[i] = best_t;
    }
}
}

namespace ml::detail::solve_intercept_times_soa_loop {
void solve_intercept_times(TArrayView<float> const out_intercept_times,
                           FVectors3f::ConstView const shooter_positions,
                           FVectors3f::ConstView const target_positions,
                           FVectors3f::ConstView const target_velocities,
                           float const projectile_speed) {
    using Vec3 = FVector3f;
    using value_type = float;

    value_type constexpr no_intercept{};
    value_type constexpr eps{1e-8f};

    auto const count{out_intercept_times.Num()};
    auto* const out{out_intercept_times.GetData()};

    check(ml::all_num_equal_and_pointers_not_equal(
        out_intercept_times, shooter_positions, target_positions, target_velocities));

    auto const projectile_speed_sq{projectile_speed * projectile_speed};
    constexpr value_type max{TNumericLimits<value_type>::Max()};

    for (int32 i{0}; i < count; ++i) {
        auto const rx{target_positions.xs[i] - shooter_positions.xs[i]};
        auto const ry{target_positions.ys[i] - shooter_positions.ys[i]};
        auto const rz{target_positions.zs[i] - shooter_positions.zs[i]};

        auto const tv_x{target_velocities.xs[i]};
        auto const tv_y{target_velocities.ys[i]};
        auto const tv_z{target_velocities.zs[i]};

        value_type const a{ml::size_sq(target_velocities, i) - projectile_speed_sq};
        value_type const b{2.0f * dot_product(rx, ry, rz, tv_x, tv_y, tv_z)};
        value_type const c{ml::size_sq(rx, ry, rz)};

        out[i] = no_intercept;

        if (FMath::Abs(a) < eps) {
            if (FMath::Abs(b) < eps) {
                continue;
            }

            value_type const t{-c / b};
            if (t > 0.f) {
                out[i] = t;
                continue;
            }

            continue;
        }

        value_type const discriminant{b * b - 4.0f * a * c};
        if (discriminant < 0.f) {
            continue;
        }

        value_type const sqrt_discriminant{FMath::Sqrt(discriminant)};

        value_type const k{0.5f / a};

        value_type const t0{k * (-b - sqrt_discriminant)};
        value_type const t1{k * (-b + sqrt_discriminant)};

        value_type best_t{max};

        if (t0 > 0.f) {
            best_t = t0;
        }

        if ((t1 > 0.f) && (t1 < best_t)) {
            best_t = t1;
        }

        if (best_t == max) {
            continue;
        }

        out[i] = best_t;
    }
}
}
