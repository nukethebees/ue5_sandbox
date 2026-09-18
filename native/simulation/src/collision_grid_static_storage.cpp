#include "ioj/sim/collision_grid_static_storage.h"

#include <ioj/sim/profiling.h>
#include "ioj/sim/world_aabb_operations.h"

#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>

namespace ioj::sim::collision {
void CollisionGridStaticStorage::reset() noexcept {
    aabbs_.reset();
    cell_range_indices_.clear();
    range_offsets_.clear();
    range_counts_.clear();
    aabb_indices_.clear();
}

void CollisionGridStaticStorage::set_aabbs(WorldAABBs aabbs) noexcept {
    aabbs_ = std::move(aabbs);
}

auto CollisionGridStaticStorage::add_aabb(Vector3f const min_point, Vector3f const max_point)
    -> std::int32_t {
    return collision::add(aabbs_, min_point, max_point);
}

auto CollisionGridStaticStorage::rebuild(GridGeometry const geometry)
    -> std::expected<void, StaticGridBuildError> {
    SANDBOX_PROFILE_SCOPE("CollisionGridStaticStorage::rebuild");

    assert(is_configured(geometry));

    range_offsets_.clear();
    range_counts_.clear();
    aabb_indices_.clear();

    auto const cell_count{geometry.dimensions.x * geometry.dimensions.y * geometry.dimensions.z};
    auto const row_stride{geometry.dimensions.x};
    auto const plane_stride{row_stride * geometry.dimensions.y};
    cell_range_indices_.assign(static_cast<std::size_t>(cell_count), -1);
    std::vector<std::int32_t> cell_counts(static_cast<std::size_t>(cell_count));

    auto const aabbs{aabbs_.get_const_view().columns()};
    auto const aabb_count{aabbs.num()};
    for (std::int32_t aabb_index{}; aabb_index < aabb_count; ++aabb_index) {
        auto const [min_cell, max_cell]{
            to_cell_coord_bounds(geometry, min_at(aabbs, aabb_index), max_at(aabbs, aabb_index))};
        if (!is_cell_coord_in_bounds(geometry, min_cell) ||
            !is_cell_coord_in_bounds(geometry, max_cell)) {
            return std::unexpected{StaticGridBuildError{
                .code = StaticGridBuildErrorCode::AabbOutOfBounds,
                .aabb_index = aabb_index,
            }};
        }

        auto plane_index{min_cell.x + min_cell.y * row_stride + min_cell.z * plane_stride};
        for (auto z{min_cell.z}; z <= max_cell.z; ++z) {
            auto row_index{plane_index};
            for (auto y{min_cell.y}; y <= max_cell.y; ++y) {
                auto cell_index{row_index};
                for (auto x{min_cell.x}; x <= max_cell.x; ++x, ++cell_index) {
                    ++cell_counts[static_cast<std::size_t>(cell_index)];
                }
                row_index += row_stride;
            }
            plane_index += plane_stride;
        }
    }

    std::int64_t membership_count{};
    for (std::int32_t cell_index{}; cell_index < cell_count; ++cell_index) {
        auto const count{cell_counts[static_cast<std::size_t>(cell_index)]};
        if (count == 0) {
            continue;
        }
        if (count > std::numeric_limits<std::uint16_t>::max()) {
            return std::unexpected{StaticGridBuildError{
                .code = StaticGridBuildErrorCode::CellCountOverflow,
                .cell_index = cell_index,
                .count = count,
            }};
        }
        if (membership_count + count > std::numeric_limits<std::int32_t>::max()) {
            return std::unexpected{StaticGridBuildError{
                .code = StaticGridBuildErrorCode::MembershipCountOverflow,
                .count = membership_count + count,
            }};
        }

        cell_range_indices_[static_cast<std::size_t>(cell_index)] =
            static_cast<std::int32_t>(range_offsets_.size());
        range_offsets_.push_back(static_cast<std::uint32_t>(membership_count));
        range_counts_.push_back(static_cast<std::uint16_t>(count));
        membership_count += count;
    }

    aabb_indices_.resize(static_cast<std::size_t>(membership_count));
    auto write_indices{range_offsets_};
    for (std::int32_t aabb_index{}; aabb_index < aabb_count; ++aabb_index) {
        auto const [min_cell, max_cell]{
            to_cell_coord_bounds(geometry, min_at(aabbs, aabb_index), max_at(aabbs, aabb_index))};
        auto plane_index{min_cell.x + min_cell.y * row_stride + min_cell.z * plane_stride};
        for (auto z{min_cell.z}; z <= max_cell.z; ++z) {
            auto row_index{plane_index};
            for (auto y{min_cell.y}; y <= max_cell.y; ++y) {
                auto cell_index{row_index};
                for (auto x{min_cell.x}; x <= max_cell.x; ++x, ++cell_index) {
                    auto const range_index{
                        cell_range_indices_[static_cast<std::size_t>(cell_index)]};
                    auto& write_index{write_indices[static_cast<std::size_t>(range_index)]};
                    aabb_indices_[static_cast<std::size_t>(write_index++)] = aabb_index;
                }
                row_index += row_stride;
            }
            plane_index += plane_stride;
        }
    }

    auto const range_count{static_cast<std::int32_t>(range_offsets_.size())};
    for (std::int32_t range_index{}; range_index < range_count; ++range_index) {
        auto const element{static_cast<std::size_t>(range_index)};
        if (write_indices[element] != range_offsets_[element] + range_counts_[element]) {
            return std::unexpected{StaticGridBuildError{
                .code = StaticGridBuildErrorCode::InconsistentWriteIndex,
                .cell_index = range_index,
            }};
        }
    }
    return {};
}

auto CollisionGridStaticStorage::aabbs() const noexcept -> WorldAABBs const& {
    return aabbs_;
}

auto CollisionGridStaticStorage::aabb_indices_for_cell(std::int32_t const cell_index) const noexcept
    -> std::span<std::int32_t const> {
    if (cell_index < 0 || static_cast<std::size_t>(cell_index) >= cell_range_indices_.size()) {
        return {};
    }

    auto const range_index{cell_range_indices_[static_cast<std::size_t>(cell_index)]};
    if (range_index < 0) {
        return {};
    }
    auto const element{static_cast<std::size_t>(range_index)};
    return std::span{aabb_indices_}.subspan(range_offsets_[element], range_counts_[element]);
}
} // namespace collision
