#include <SandboxCore/projectile_intercept.h>

namespace ml {
auto solve_intercept_time(FVector const& shooter_pos,
                          FVector const& target_pos,
                          FVector const& target_vel,
                          float const projectile_speed) -> float {
    using value_type = float;

    value_type constexpr no_intercept{-1.f};
    value_type constexpr eps{1e-8f};

    FVector const r{target_pos - shooter_pos};

    value_type const a{static_cast<value_type>(target_vel.SizeSquared()) -
                       projectile_speed * projectile_speed};
    value_type const b{2.0f * static_cast<value_type>(FVector::DotProduct(r, target_vel))};
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
}
