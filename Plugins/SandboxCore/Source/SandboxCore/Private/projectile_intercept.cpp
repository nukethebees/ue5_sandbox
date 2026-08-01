#include <SandboxCore/projectile_intercept.h>

namespace ml {
auto solve_intercept_time(FVector3f const& shooter_pos,
                          FVector3f const& target_pos,
                          FVector3f const& target_vel,
                          float const projectile_speed) -> float {
    using Vec3 = FVector3f;
    using value_type = float;

    value_type constexpr no_intercept{-1.f};
    value_type constexpr eps{1e-8f};

    Vec3 const r{target_pos - shooter_pos};

    value_type const a{static_cast<value_type>(target_vel.SizeSquared()) -
                       projectile_speed * projectile_speed};
    value_type const b{2.0f * static_cast<value_type>(Vec3::DotProduct(r, target_vel))};
    value_type const c{static_cast<value_type>(r.SizeSquared())};

    if (FMath::Abs(a) < eps) {
        if (FMath::Abs(b) < eps) {
            return {};
        }

        value_type const t{-c / b};
        if (t > 0.f) {
            return t;
        }

        return {};
    }

    value_type const discriminant{b * b - 4.0f * a * c};
    if (discriminant < 0.f) {
        return {};
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

void solve_intercept_times(TArrayView<float> const out_intercept_times,
                           FVectors3f::ConstView const shooter_positions,
                           FVectors3f::ConstView const target_positions,
                           FVectors3f::ConstView const target_velocities,
                           float const projectile_speed) {
    auto const count{out_intercept_times.Num()};

    check(shooter_positions.num() == count);
    check(target_positions.num() == count);
    check(target_velocities.num() == count);
    check(shooter_positions.ys.Num() == count);
    check(shooter_positions.zs.Num() == count);
    check(target_positions.ys.Num() == count);
    check(target_positions.zs.Num() == count);
    check(target_velocities.ys.Num() == count);
    check(target_velocities.zs.Num() == count);

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
