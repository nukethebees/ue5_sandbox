#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/collision_grid_entity_storage.h"
#include "ioj/sim/collision_grid_static_storage.h"
#include "ioj/sim/entity_registry_query.h"
#include "ioj/sim/entity_world_bounds.h"
#include "ioj/sim/registry_entity_handles.h"

#include <cstdint>
#include <vector>

namespace ioj::sim::collision {
void append_grid_overlaps(GridGeometry geometry,
                          CollisionGridEntityStorage const& entity_storage,
                          CollisionGridStaticStorage const& static_storage,
                          EntityRegistryQueryView registry,
                          WorldAABB query_bounds,
                          RegistryEntityHandle ignored_entity,
                          std::vector<RegistryEntityHandle>& out_entities,
                          std::vector<std::int32_t>& out_static_geometry_indices);
}
