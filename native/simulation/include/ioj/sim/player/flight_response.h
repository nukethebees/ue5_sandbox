#pragma once

#include "ioj/sim/sim_time.h"
#include "ioj/sim/step_response.h"

#include <cmath>

namespace ioj::sim {
class FlightResponse {
  public:
    auto update(SimTime const dt) -> float {
        time_ += dt;
        auto const delta{(target_value_ - initial_value_) *
                         response_curve_.value_at(static_cast<float>(time_))};
        return initial_value_ + delta;
    }

    void reset(SimTime const settling_time,
               float damping_ratio,
               float const initial_value,
               float const target_value) {
        initial_value_ = initial_value;
        target_value_ = target_value;
        time_ = 0.f;

        if (std::abs(1.f - damping_ratio) < 1e-6) {
            damping_ratio = 0.9999f;
        }
        response_curve_.configure(static_cast<float>(settling_time), damping_ratio);
    }
  private:
    SimTime time_{};
    float initial_value_{};
    float target_value_{};
    DampedStepResponse response_curve_{};
};

}
