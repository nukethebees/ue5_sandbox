#pragma once

#include <ioj/sim/collision/collision_grid_spec.h>
#include <ioj/sim/collision/entity_aabbs.h>
#include <ioj/sim/collision/overlap_response_config.h>
#include <ioj/sim/collision/world_aabbs.h>

namespace ioj::sim {
struct CollisionInitData {
    collision::EntityAABBs entity_bounds{};
    collision::WorldAABBs static_bounds{};
    collision::CollisionGridSpec grid_spec{{400, 400, 5}, Vector3f{5000.f, 5000.f, 20000.f}};
    OverlapResponseConfig overlap_response{};
};
} // namespace ioj::sim
