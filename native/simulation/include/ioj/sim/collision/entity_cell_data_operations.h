#pragma once

#include "ioj/math_types.h"
#include "ioj/sim/collision/collision_grid.h"
#include "ioj/sim/collision/entity_cell_data.h"

#include <cassert>
#include <span>

namespace ioj::sim::collision {
// Keep storage fixed while this writer is in use; index must name an allocated row.
struct EntityCellDataWriter {
    explicit EntityCellDataWriter(EntityCellData::View const view)
        : min_point_xs_{view.min_point_xs()}
        , min_point_ys_{view.min_point_ys()}
        , min_point_zs_{view.min_point_zs()}
        , max_point_xs_{view.max_point_xs()}
        , max_point_ys_{view.max_point_ys()}
        , max_point_zs_{view.max_point_zs()}
        , min_cell_xs_{view.min_cell_xs()}
        , min_cell_ys_{view.min_cell_ys()}
        , min_cell_zs_{view.min_cell_zs()}
        , max_cell_xs_{view.max_cell_xs()}
        , max_cell_ys_{view.max_cell_ys()}
        , max_cell_zs_{view.max_cell_zs()}
        , entity_ids_{view.entity_ids()} {}

    void set(std::uint32_t const index,
             Vector3f const min_point,
             Vector3f const max_point,
             CellCoord const min_cell,
             CellCoord const max_cell,
             EntityId const id) const {
        assert(index < entity_ids_.size());

        min_point_xs_[index] = min_point.x();
        min_point_ys_[index] = min_point.y();
        min_point_zs_[index] = min_point.z();
        max_point_xs_[index] = max_point.x();
        max_point_ys_[index] = max_point.y();
        max_point_zs_[index] = max_point.z();
        min_cell_xs_[index] = min_cell.x();
        min_cell_ys_[index] = min_cell.y();
        min_cell_zs_[index] = min_cell.z();
        max_cell_xs_[index] = max_cell.x();
        max_cell_ys_[index] = max_cell.y();
        max_cell_zs_[index] = max_cell.z();
        entity_ids_[index] = id;
    }
  private:
    std::span<float> min_point_xs_;
    std::span<float> min_point_ys_;
    std::span<float> min_point_zs_;
    std::span<float> max_point_xs_;
    std::span<float> max_point_ys_;
    std::span<float> max_point_zs_;
    std::span<std::int32_t> min_cell_xs_;
    std::span<std::int32_t> min_cell_ys_;
    std::span<std::int32_t> min_cell_zs_;
    std::span<std::int32_t> max_cell_xs_;
    std::span<std::int32_t> max_cell_ys_;
    std::span<std::int32_t> max_cell_zs_;
    std::span<EntityId> entity_ids_;
};

[[nodiscard]] inline auto min_point_at(EntityCellData::ConstView const data,
                                       std::int32_t const index) -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return Vector3f{
        data.min_point_xs()[element], data.min_point_ys()[element], data.min_point_zs()[element]};
}

[[nodiscard]] inline auto max_point_at(EntityCellData::ConstView const data,
                                       std::int32_t const index) -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return Vector3f{
        data.max_point_xs()[element], data.max_point_ys()[element], data.max_point_zs()[element]};
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
