#include "ioj/sim/attack_distance_band.h"

namespace ioj::sim {
auto AttackDistanceBand::values_are_valid() const noexcept -> bool {
    return minimum_ratio >= 0.f && minimum_ratio <= desired_ratio &&
           desired_ratio <= maximum_ratio && maximum_ratio <= 1.f;
}
} // namespace ioj::sim
