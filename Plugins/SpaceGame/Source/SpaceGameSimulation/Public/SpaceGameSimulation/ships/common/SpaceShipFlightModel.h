#pragma once

#include "CoreMinimal.h"
#include "SandboxCoreEngine/SpeedResponse.h"

#include <sandbox/simulation/step_response.h>

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

        response_curve_.configure(response.settling_time, response.damping_ratio);
    }
  protected:
    auto step_size() const -> T { return target_speed - old_speed; }

    auto calculate_delta(float const t) const -> T {
        auto const h{response_curve_.value_at(t)};

#if WITH_EDITOR
        h_dbg = h;
#endif

        return step_size() * h;
    }

    FSpeedResponse response{};
    float time{0.f};
    T old_speed{};
    T target_speed{};
    ml::simulation::DampedStepResponse response_curve_{};

#if WITH_EDITORONLY_DATA
    mutable float h_dbg{0.f};
    T step_size_original_dbg{};
    T step_size_dbg{};
    T dy_dbg{};
#endif
};
