#pragma once

#include "CoreMinimal.h"
#include "Sandbox/players/SpeedResponse.h"

template <typename T>
struct TSpaceShipFlightModel {
    TSpaceShipFlightModel() = default;
    TSpaceShipFlightModel(FSpeedResponse sr)
        : response(sr) {}

    auto update(float const dt) -> T {
        time += dt;
        auto const delta_speed{calculate_delta(time)};

#if WITH_EDITOR
        dy_dbg = delta_speed;
        step_size_dbg = step_size();
#endif

        return old_speed + delta_speed;
    }

    void set_new_impulse(FSpeedResponse sr, T const old_s, T const target_s) {
        response = sr;
        old_speed = old_s;
        target_speed = target_s;
        time = 0.f;

        // Fix issue where damping ratio is 1
        if (FMath::Abs(1.f - response.damping_ratio) < 1e-6) {
            response.damping_ratio = 0.9999f;
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
  protected:
    auto step_size() const -> T { return target_speed - old_speed; }

    auto calculate_delta(float const t) const -> T {
        auto const inner{c * FMath::Cos(wd * t + delta)};
        auto const h{1.f - FMath::Exp(-z_wn * t) * inner};

#if WITH_EDITOR
        h_dbg = h;
#endif

        return step_size() * h;
    }

    FSpeedResponse response{};
    float time{0.f};
    T old_speed{};
    T target_speed{};
    float alpha{0.f};
    float wn{0.f};
    float wd{0.f};
    float z_wn{0.f};
    // Harmonic addition theorem
    // sin(x) + b*cos(x) -> c * cos(x + delta)
    float delta{0.f};
    float c{0.f};

#if WITH_EDITORONLY_DATA
    mutable float h_dbg{0.f};
    T step_size_original_dbg{};
    T step_size_dbg{};
    T dy_dbg{};
#endif
};
