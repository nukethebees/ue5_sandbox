#pragma once

#include "ioj/sim/step_response.h"

#include <cmath>

namespace ioj::sim {
template <typename T>
class ShipFlightModel {
  public:
    auto update(float const dt) -> T {
        time_ += dt;
        auto const delta_speed{(target_speed_ - old_speed_) * response_curve_.value_at(time_)};
        return old_speed_ + delta_speed;
    }

    void set_new_impulse(float const settling_time,
                         float damping_ratio,
                         T const old_speed,
                         T const target_speed) {
        old_speed_ = old_speed;
        target_speed_ = target_speed;
        time_ = 0.f;

        if (std::abs(1.f - damping_ratio) < 1e-6) {
            damping_ratio = 0.9999f;
        }
        response_curve_.configure(settling_time, damping_ratio);
    }
  private:
    float time_{};
    T old_speed_{};
    T target_speed_{};
    DampedStepResponse response_curve_{};
};

extern template class ShipFlightModel<float>;
}
