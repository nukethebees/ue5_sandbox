#pragma once

#include "sandbox/simulation/entity_aabbs.h"
#include "sandbox/simulation/vector_types.h"

#include <cstdint>

namespace ml::simulation::collision {
struct WorldAABB {
    Vector3f min;
    Vector3f max;
};

[[nodiscard]] auto make_entity_world_bounds(EntityAABBs const& bounds,
                                            std::int32_t type_index,
                                            Vector3f position,
                                            Quaternion4f orientation) noexcept -> WorldAABB;
} // namespace ml::simulation::collision
