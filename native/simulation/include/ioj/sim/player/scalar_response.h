#pragma once

#include <ioj/sim/player/flight_model_config.h>
#include <ioj/sim/ship_flight_model.h>

namespace ioj::sim::player {
class ScalarResponse {
  public:
    void reset(float value) noexcept;
    [[nodiscard]] auto update(float dt, float target, ResponseConfig const& config) noexcept
        -> float;
    [[nodiscard]] auto value() const noexcept -> float { return value_; }
  private:
    void begin_second_order_impulse(float target, SecondOrderResponseConfig const& config) noexcept;

    ShipFlightModel<float> second_order_{};
    float value_{};
    float target_{};
    ResponseMode mode_{ResponseMode::Direct};
    bool initialized_{};
};
}
