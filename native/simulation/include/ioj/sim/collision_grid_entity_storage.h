#pragma once

#include "ioj/sim/entity_cell_data.h"
#include "ioj/sim/entity_unique_id.h"
#include "ioj/sim/world_aabbs.h"

#include <cstdint>
#include <vector>

namespace ioj::sim::collision {
struct CollisionGridEntityStorage {
    std::vector<std::int32_t> cell_offsets;
    std::vector<std::uint16_t> cell_counts;
    std::vector<std::int32_t> non_empty_cell_indices;
    std::vector<EntityUniqueId> entities;
    WorldAABBs aabbs;

    // Rebuild-only scratch and intermediate entity data.
    std::vector<std::int32_t> cell_write_indices;
    EntityCellData rebuild_entity_data;
};
} // namespace ioj::sim::collision
