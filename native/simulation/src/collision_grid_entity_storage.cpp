#include "ioj/sim/collision_grid_entity_storage.h"

#include "ioj/sim/entity_cell_data_operations.h"
#include "ioj/sim/world_aabb_operations.h"

#include <cassert>
#include <cstddef>

namespace ioj::sim::collision {
void CollisionGridEntityStorage::reset() noexcept {
    grid_dimensions_ = {};
    cell_offsets_.clear();
    cell_counts_.clear();
    cell_write_indices_.clear();
    non_empty_cell_indices_.clear();
    entities_.clear();
    aabbs_.reset();
    entity_cells_.reset();
}

void CollisionGridEntityStorage::begin_rebuild(CellCoord const grid_dimensions) {
    assert(grid_dimensions.x > 0 && grid_dimensions.y > 0 && grid_dimensions.z > 0);

    grid_dimensions_ = grid_dimensions;
    for (auto const cell_index : non_empty_cell_indices_) {
        cell_counts_[static_cast<std::size_t>(cell_index)] = 0;
    }
    non_empty_cell_indices_.clear();

    auto const cell_count{static_cast<std::size_t>(grid_dimensions.x) *
                          static_cast<std::size_t>(grid_dimensions.y) *
                          static_cast<std::size_t>(grid_dimensions.z)};
    if (cell_counts_.size() != cell_count) {
        cell_counts_.assign(cell_count, std::uint16_t{});
    }
    cell_offsets_.resize(cell_count);
    cell_write_indices_.resize(cell_count);
    entity_cells_.reset();
}

void CollisionGridEntityStorage::add(Vector3f const min_point,
                                     Vector3f const max_point,
                                     CellCoord const min_cell,
                                     CellCoord const max_cell,
                                     EntityUniqueId const id) {
    [[maybe_unused]] auto const in_bounds{[this](CellCoord const cell) {
        return cell.x >= 0 && cell.x < grid_dimensions_.x && cell.y >= 0 &&
               cell.y < grid_dimensions_.y && cell.z >= 0 && cell.z < grid_dimensions_.z;
    }};
    assert(in_bounds(min_cell));
    assert(in_bounds(max_cell));

    collision::add(entity_cells_, min_point, max_point, min_cell, max_cell, id);

    auto const row_stride{grid_dimensions_.x};
    auto const plane_stride{row_stride * grid_dimensions_.y};
    auto plane_index{min_cell.x + min_cell.y * row_stride + min_cell.z * plane_stride};
    for (auto z{min_cell.z}; z <= max_cell.z; ++z) {
        auto row_index{plane_index};
        for (auto y{min_cell.y}; y <= max_cell.y; ++y) {
            auto cell_index{row_index};
            for (auto x{min_cell.x}; x <= max_cell.x; ++x, ++cell_index) {
                auto& count{cell_counts_[static_cast<std::size_t>(cell_index)]};
                if (count == 0) {
                    non_empty_cell_indices_.push_back(cell_index);
                }
                ++count;
            }
            row_index += row_stride;
        }
        plane_index += plane_stride;
    }
}

auto CollisionGridEntityStorage::finish_rebuild() -> bool {
    std::int32_t entry_count{};
    for (auto const cell_index : non_empty_cell_indices_) {
        auto const element{static_cast<std::size_t>(cell_index)};
        cell_offsets_[element] = entry_count;
        cell_write_indices_[element] = entry_count;
        entry_count += cell_counts_[element];
    }

    aabbs_.reset();
    aabbs_.add_uninitialised(entry_count);
    entities_.resize(static_cast<std::size_t>(entry_count));

    auto const entity_cells{entity_cells_.get_const_view().columns()};
    auto const entity_count{entity_cells.num()};
    auto const row_stride{grid_dimensions_.x};
    auto const plane_stride{row_stride * grid_dimensions_.y};
    for (std::int32_t entity_index{}; entity_index < entity_count; ++entity_index) {
        auto const min_cell{min_cell_at(entity_cells, entity_index)};
        auto const max_cell{max_cell_at(entity_cells, entity_index)};
        auto const min_point{min_point_at(entity_cells, entity_index)};
        auto const max_point{max_point_at(entity_cells, entity_index)};

        auto plane_index{min_cell.x + min_cell.y * row_stride + min_cell.z * plane_stride};
        for (auto z{min_cell.z}; z <= max_cell.z; ++z) {
            auto row_index{plane_index};
            for (auto y{min_cell.y}; y <= max_cell.y; ++y) {
                auto cell_index{row_index};
                for (auto x{min_cell.x}; x <= max_cell.x; ++x, ++cell_index) {
                    auto& write_index{cell_write_indices_[static_cast<std::size_t>(cell_index)]};
                    auto const destination{write_index++};
                    entities_[static_cast<std::size_t>(destination)] =
                        entity_cells.entity_ids[static_cast<std::size_t>(entity_index)];
                    collision::set(aabbs_, destination, min_point, max_point);
                }
                row_index += row_stride;
            }
            plane_index += plane_stride;
        }
    }

    for (auto const cell_index : non_empty_cell_indices_) {
        auto const element{static_cast<std::size_t>(cell_index)};
        if (cell_write_indices_[element] != cell_offsets_[element] + cell_counts_[element]) {
            return false;
        }
    }
    return true;
}

auto CollisionGridEntityStorage::non_empty_cell_count() const noexcept -> std::int32_t {
    return static_cast<std::int32_t>(non_empty_cell_indices_.size());
}

auto CollisionGridEntityStorage::entities_for_cell(std::int32_t const cell_index) const noexcept
    -> std::span<EntityUniqueId const> {
    assert(cell_index >= 0 && static_cast<std::size_t>(cell_index) < cell_counts_.size());

    auto const element{static_cast<std::size_t>(cell_index)};
    auto const count{cell_counts_[element]};
    if (count == 0) {
        return {};
    }
    return std::span{entities_}.subspan(static_cast<std::size_t>(cell_offsets_[element]), count);
}

auto CollisionGridEntityStorage::aabbs_for_cell(std::int32_t const cell_index) const noexcept
    -> WorldAABBsColumnsConstView {
    assert(cell_index >= 0 && static_cast<std::size_t>(cell_index) < cell_counts_.size());

    auto const element{static_cast<std::size_t>(cell_index)};
    return aabbs_.get_const_view(cell_offsets_[element], cell_counts_[element]).columns();
}

auto CollisionGridEntityStorage::entity_world_bounds() const noexcept
    -> WorldAABBsColumnsConstView {
    auto const cells{entity_cells_.get_const_view().columns()};
    return {cells.min_point_xs,
            cells.min_point_ys,
            cells.min_point_zs,
            cells.max_point_xs,
            cells.max_point_ys,
            cells.max_point_zs};
}

} // namespace collision
