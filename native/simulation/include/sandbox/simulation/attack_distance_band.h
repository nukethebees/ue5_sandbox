#pragma once

namespace ml::simulation {
struct AttackDistanceBand {
    [[nodiscard]] auto values_are_valid() const noexcept -> bool;

    float minimum_ratio{0.4f};
    float desired_ratio{0.5f};
    float maximum_ratio{0.6f};
};
} // namespace ml::simulation
