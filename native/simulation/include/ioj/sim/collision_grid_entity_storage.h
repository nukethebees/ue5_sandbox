#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/entity_cell_data.h"
#include "ioj/sim/entity_unique_id.h"
#include "ioj/sim/world_aabbs.h"

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim::collision {
class CollisionGridEntityStorage {
  public:
    void reset() noexcept;
    void begin_rebuild(CellCoord grid_dimensions);
    void add(Vector3f min_point,
             Vector3f max_point,
             CellCoord min_cell,
             CellCoord max_cell,
             EntityUniqueId id);
    [[nodiscard]] auto finish_rebuild() -> bool;

    [[nodiscard]] auto non_empty_cell_count() const noexcept -> std::int32_t;
    [[nodiscard]] auto entities_for_cell(std::int32_t cell_index) const noexcept
        -> std::span<EntityUniqueId const>;
    [[nodiscard]] auto aabbs_for_cell(std::int32_t cell_index) const noexcept
        -> WorldAABBsColumnsConstView;
    [[nodiscard]] auto entity_world_bounds() const noexcept -> WorldAABBsColumnsConstView;
  private:
    CellCoord grid_dimensions_{};
    std::vector<std::int32_t> cell_offsets_;
    std::vector<std::uint16_t> cell_counts_;
    std::vector<std::int32_t> cell_write_indices_;
    std::vector<std::int32_t> non_empty_cell_indices_;
    std::vector<EntityUniqueId> entities_;
    WorldAABBs aabbs_;
    EntityCellData entity_cells_;
};
} // namespace ioj::sim::collision
