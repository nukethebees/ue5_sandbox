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
    auto const view{data.get_view()};
    view.min_point_xs()[index] = min_point.X;
    view.min_point_ys()[index] = min_point.Y;
    view.min_point_zs()[index] = min_point.Z;
    view.max_point_xs()[index] = max_point.X;
    view.max_point_ys()[index] = max_point.Y;
    view.max_point_zs()[index] = max_point.Z;
    view.min_cell_xs()[index] = min_cell.x;
    view.min_cell_ys()[index] = min_cell.y;
    view.min_cell_zs()[index] = min_cell.z;
    view.max_cell_xs()[index] = max_cell.x;
    view.max_cell_ys()[index] = max_cell.y;
    view.max_cell_zs()[index] = max_cell.z;
    view.entity_ids()[index] = id;
}

[[nodiscard]] inline auto min_point_at(EntityCellData::ConstView const data,
                                       std::int32_t const index) -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return ml::make_vector3f(
        data.min_point_xs()[element], data.min_point_ys()[element], data.min_point_zs()[element]);
}

[[nodiscard]] inline auto max_point_at(EntityCellData::ConstView const data,
                                       std::int32_t const index) -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return ml::make_vector3f(
        data.max_point_xs()[element], data.max_point_ys()[element], data.max_point_zs()[element]);
}

[[nodiscard]] inline auto min_cell_at(EntityCellData::ConstView const data,
                                      std::int32_t const index) -> CellCoord {
    auto const element{static_cast<std::size_t>(index)};
    return {data.min_cell_xs()[element], data.min_cell_ys()[element], data.min_cell_zs()[element]};
}

[[nodiscard]] inline auto max_cell_at(EntityCellData::ConstView const data,
                                      std::int32_t const index) -> CellCoord {
    auto const element{static_cast<std::size_t>(index)};
    return {data.max_cell_xs()[element], data.max_cell_ys()[element], data.max_cell_zs()[element]};
}
} // namespace ioj::sim::collision
