#include "Sandbox/players/SpaceShipFlightModel.h"

#include "CoreMinimal.h"

auto FSpaceShipFlightModel::calculate_dy(float t) const -> float {
    auto const wd_t{wd * t};
    auto const inner{c * FMath::Cos(wd * t + delta)};
    auto const h{1.f - FMath::Exp(-z_wn * t) * inner};

#if WITH_EDITOR
    h_dbg = h;
#endif

    return step_size() * h;
}
auto FSpaceShipFlightModel::update_y(float dt) -> float {
    time += dt;
    auto const delta_speed{calculate_dy(time)};

#if WITH_EDITOR
    dy_dbg = delta_speed;
    step_size_dbg = step_size();
#endif

    return old_speed + delta_speed;
}
void FSpaceShipFlightModel::set_new_impulse(FSpeedResponse sr, float old_s, float target_s) {
    response = sr;
    old_speed = old_s;
    target_speed = target_s;
    time = 0.f;

    // Fix issue where damping ratio is 1
    if (FMath::Abs(1.f - response.damping_ratio) < 1e-6) {
        response.damping_ratio = 0.9999;
    }

#if WITH_EDITOR
    step_size_original_dbg = step_size();
#endif

    auto const z{response.damping_ratio};
    wn = response.natural_angular_frequency();
    auto const x{FMath::Sqrt(1 - z * z)};
    wd = wn * x;
    alpha = z / x;
    z_wn = response.damping_ratio * wn;

    delta = FMath::Atan2(-alpha, 1.f);
    c = FMath::Sqrt(1.f + alpha * alpha);
}
