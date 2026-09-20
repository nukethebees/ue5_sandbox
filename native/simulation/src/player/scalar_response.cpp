#include "ioj/sim/player/scalar_response.h"

#include <algorithm>
#include <cmath>

namespace ioj::sim::player {
void ScalarResponse::reset(float const value) noexcept {
    value_ = value;
    target_ = value;
    initialized_ = false;
}

auto ScalarResponse::update(float const dt,
                            float const target,
                            ResponseConfig const& config) noexcept -> float {
    if (!initialized_ || mode_ != config.mode) {
        target_ = target;
        mode_ = config.mode;
        initialized_ = true;
        if (mode_ == ResponseMode::SecondOrder) {
            begin_second_order_impulse(target, config.second_order);
        }
    }

    switch (mode_) {
        case ResponseMode::Direct:
            value_ = target;
            break;
        case ResponseMode::RateLimited: {
            auto const increasing{value_ * target >= 0.f && std::abs(target) > std::abs(value_)};
            auto const rate{increasing ? config.rate_limited.increasing_rate
                                       : config.rate_limited.decreasing_rate};
            value_ = std::clamp(target, value_ - rate * dt, value_ + rate * dt);
            break;
        }
        case ResponseMode::SecondOrder:
            if (target != target_) {
                begin_second_order_impulse(target, config.second_order);
            }
            value_ = second_order_.update(dt);
            break;
    }

    target_ = target;
    return value_;
}

void ScalarResponse::begin_second_order_impulse(float const target,
                                                SecondOrderResponseConfig const& config) noexcept {
    second_order_.set_new_impulse(config.settling_time, config.damping_ratio, value_, target);
}
}
