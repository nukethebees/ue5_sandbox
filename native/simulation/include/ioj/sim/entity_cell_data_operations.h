#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/entity_cell_data.h"
#include "ioj/sim/vector_types.h"

namespace ioj::sim::collision {
inline void add(EntityCellData& data,
                Vector3f const min_point,
                Vector3f const max_point,
                CellCoord const min_cell,
                CellCoord const max_cell,
                EntityUniqueId const id) {
    auto const index{data.num()};
    data.add_uninitialised(1);
    data.get_view().columns().set(index,
                                  min_point.X,
                                  min_point.Y,
                                  min_point.Z,
                                  max_point.X,
                                  max_point.Y,
                                  max_point.Z,
                                  min_cell.x,
                                  min_cell.y,
                                  min_cell.z,
                                  max_cell.x,
                                  max_cell.y,
                                  max_cell.z,
                                  id);
}

[[nodiscard]] inline auto min_point_at(EntityCellDataColumnsConstView const& data,
                                       std::int32_t const index) -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return ml::make_vector3f(
        data.min_point_xs[element], data.min_point_ys[element], data.min_point_zs[element]);
}

[[nodiscard]] inline auto max_point_at(EntityCellDataColumnsConstView const& data,
                                       std::int32_t const index) -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return ml::make_vector3f(
        data.max_point_xs[element], data.max_point_ys[element], data.max_point_zs[element]);
}

[[nodiscard]] inline auto min_cell_at(EntityCellDataColumnsConstView const& data,
                                      std::int32_t const index) -> CellCoord {
    auto const element{static_cast<std::size_t>(index)};
    return {data.min_cell_xs[element], data.min_cell_ys[element], data.min_cell_zs[element]};
}

[[nodiscard]] inline auto max_cell_at(EntityCellDataColumnsConstView const& data,
                                      std::int32_t const index) -> CellCoord {
    auto const element{static_cast<std::size_t>(index)};
    return {data.max_cell_xs[element], data.max_cell_ys[element], data.max_cell_zs[element]};
}
} // namespace ioj::sim::collision
