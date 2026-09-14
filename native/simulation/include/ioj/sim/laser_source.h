#pragma once

#include "ioj/sim/entity_types.h"

namespace ioj::sim {
struct LaserSource {
    Team team{Team::White};
    EntityType type{EntityType::TubeSpinner};

    auto operator==(LaserSource const&) const noexcept -> bool = default;
};
}
