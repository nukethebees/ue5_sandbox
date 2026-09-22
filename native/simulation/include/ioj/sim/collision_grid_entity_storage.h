#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/entity_cell_data.h"
#include "ioj/sim/entity_unique_id.h"
#include "ioj/sim/world_aabbs.h"

#include <cstdint>
#include <vector>

namespace ioj::sim::collision {
struct CollisionGridEntityStorage {
    using CellEntryOffset = std::int32_t;
    using CellEntryCount = std::uint16_t;

    /* **************************************** */
    // Grid contents
    /* **************************************** */
    std::vector<CellEntryOffset> cell_offsets;
    std::vector<CellEntryCount> cell_counts;
    std::vector<CellIndex> non_empty_cell_indices;
    std::vector<EntityUniqueId> entities;
    WorldAABBs aabbs;

    /* **************************************** */
    // Rebuild scratch
    /* **************************************** */
    std::vector<CellEntryOffset> cell_write_indices;
    EntityCellData rebuild_entity_data;
};
} // namespace ioj::sim::collision
