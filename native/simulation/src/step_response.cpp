#include "sandbox/simulation/step_response.h"

#include <cmath>

namespace ml::simulation {
void DampedStepResponse::configure(float const settling_time, float const damping_ratio) noexcept {
    auto const natural_frequency{5.0f / settling_time};
    auto const root{std::sqrt(1.0f - damping_ratio * damping_ratio)};
    damped_frequency_ = natural_frequency * root;
    alpha_ = damping_ratio / root;
    decay_rate_ = damping_ratio * natural_frequency;
    phase_ = std::atan2(-alpha_, 1.0f);
    magnitude_ = std::sqrt(1.0f + alpha_ * alpha_);
}

auto DampedStepResponse::value_at(float const time) const noexcept -> float {
    auto const oscillation{magnitude_ * std::cos(damped_frequency_ * time + phase_)};
    return 1.0f - std::exp(-decay_rate_ * time) * oscillation;
}

auto clamp_health_to_max(int const health, int const max_health) noexcept -> int {
    return max_health > health ? max_health : health;
}
}
