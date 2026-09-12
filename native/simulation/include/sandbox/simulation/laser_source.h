#pragma once

#include "sandbox/simulation/entity_types.h"

namespace ml::simulation {
struct LaserSource {
    Team team{Team::White};
    EntityType type{EntityType::TubeSpinner};

    auto operator==(LaserSource const&) const noexcept -> bool = default;
};
}
