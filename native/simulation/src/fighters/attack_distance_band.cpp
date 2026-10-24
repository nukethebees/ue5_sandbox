#include "ioj/sim/fighters/attack_distance_band.h"

namespace ioj::sim {
auto AttackDistanceBand::make(float const minimum, float const desired, float const maximum)
    -> std::expected<AttackDistanceBand, BandError> {
    if (!(minimum >= 0.f && minimum <= desired && desired <= maximum && maximum <= 1.f)) {
        return std::unexpected{BandError::InvalidRatios};
    }

    AttackDistanceBand result;
    result.minimum_ = minimum;
    result.desired_ = desired;
    result.maximum_ = maximum;
    return result;
}
} // namespace ioj::sim
