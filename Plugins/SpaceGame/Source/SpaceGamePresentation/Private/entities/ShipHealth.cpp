#include "SpaceGamePresentation/entities/ShipHealth.h"

#include <algorithm>

namespace ioj {
void FShipHealth::clamp_to_max() noexcept {
    health = std::max(health, max_health);
}

} // namespace ioj
