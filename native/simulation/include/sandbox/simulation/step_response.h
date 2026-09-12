#pragma once

namespace ml::simulation {
class DampedStepResponse {
  public:
    void configure(float settling_time, float damping_ratio) noexcept;
    [[nodiscard]] auto value_at(float time) const noexcept -> float;
  private:
    float alpha_{};
    float damped_frequency_{};
    float decay_rate_{};
    float phase_{};
    float magnitude_{};
};

[[nodiscard]] auto clamp_health_to_max(int health, int max_health) noexcept -> int;
}
