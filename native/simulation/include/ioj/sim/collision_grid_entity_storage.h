#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/entity_cell_data.h"
#include "ioj/sim/entity_unique_id.h"
#include "ioj/sim/world_aabbs.h"

#include <sandbox/core/enum_array.h>

#include <cstdint>
#include <vector>

namespace ioj::sim::collision {
struct CollisionGridEntityStorage {
    using CellEntryOffset = std::uint32_t;
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
    // Map each per-type entity-storage row to its cached world-space AABB row.
    // Rows omitted from the rebuild have EntityInstanceHandle::invalid_value.
    ml::EnumArray<EntityType, std::vector<std::uint32_t>> entity_row_to_aabb_row;
};
} // namespace ioj::sim::collision
