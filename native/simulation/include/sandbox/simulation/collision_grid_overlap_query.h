#pragma once

#include "sandbox/simulation/collision_grid.h"
#include "sandbox/simulation/collision_grid_entity_storage.h"
#include "sandbox/simulation/collision_grid_static_storage.h"
#include "sandbox/simulation/entity_registry_query.h"
#include "sandbox/simulation/entity_world_bounds.h"
#include "sandbox/simulation/registry_entity_handles.h"

#include <cstdint>
#include <vector>

namespace ml::simulation::collision {
void append_grid_overlaps(GridGeometry geometry,
                          CollisionGridEntityStorage const& entity_storage,
                          CollisionGridStaticStorage const& static_storage,
                          EntityRegistryQueryView registry,
                          WorldAABB query_bounds,
                          FRegistryEntityHandle ignored_entity,
                          std::vector<FRegistryEntityHandle>& out_entities,
                          std::vector<std::int32_t>& out_static_geometry_indices);
}
