#pragma once

#include <ioj/sim/collision/trace_entity_filter.h>
#include <ioj/sim/collision_grid.h>
#include <ioj/sim/collision_grid_entity_storage.h>
#include <ioj/sim/collision_grid_static_storage.h>
#include <ioj/sim/collision_types.h>
#include <ioj/sim/entity_world_bounds.h>
#include <ioj/sim/line_trace_batch.h>
#include <ioj/sim/player_spatial_data.h>
#include <ioj/sim/system_read_views.h>
#include <ioj/sim/trace_hits.h>

#include <sandbox/core/frame_array.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ioj::sim {
struct EntityTables;
}

namespace ioj::sim::collision {

struct CollisionUniformGrid {
    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    CollisionUniformGrid() = default;
    CollisionUniformGrid(CollisionUniformGrid const&) = delete;
    CollisionUniformGrid(CollisionUniformGrid&&) = delete;
    auto operator=(CollisionUniformGrid const&) -> CollisionUniformGrid& = delete;
    auto operator=(CollisionUniformGrid&&) -> CollisionUniformGrid& = delete;
    void reset();

    /* **************************************** */
    // Grid geometry
    /* **************************************** */
    auto is_configured() const noexcept -> bool;
    void set_geometry(GridGeometry geometry) noexcept;
    auto get_grid_dims() const noexcept -> CellCoord;
    auto get_max_grid_coord() const noexcept -> CellCoord;
    auto get_cell_dims() const noexcept -> Vector3f;
    auto num_cells() const -> GridCellCount;
    auto to_cell_coord(Vector3f pos) const -> CellCoord;
    auto to_cell_coord_bounds(Vector3f min_point, Vector3f max_point) const -> CellCoordBounds;
    auto is_cell_coord_in_bounds(CellCoord coord) const -> bool;
    auto is_cell_coord_in_bounds(CellCoord min_coord, CellCoord max_coord) const -> bool;
    void are_spheres_in_bounds(Vectors3fConstView centres,
                               float radius,
                               std::span<SphereInBoundsResult> out_results) const;
    static auto to_string(CellCoord value) -> std::string;

    /* **************************************** */
    // Static collision
    /* **************************************** */
    void set_static_aabbs(WorldAABBs static_aabbs);
    auto add_static_aabb(Vector3f min_point, Vector3f max_point) -> StaticGeometryIndex;
    auto get_static_aabbs() const noexcept -> WorldAABBs const& { return static_storage_.aabbs(); }

    /* **************************************** */
    // Entity collision
    /* **************************************** */
    void rebuild_entity_grid(EntityAABBs const& entity_aabbs,
                             CapitalReadView capitals,
                             FighterReadView fighters,
                             TurretReadView turrets,
                             SpinnerReadView spinners,
                             std::optional<PlayerSpatialData> player = {});
    auto get_entity_world_bounds() const -> EntityCellData::ConstView;
    auto bound_rows(EntityType const type) const -> std::span<std::uint32_t const> {
        return entity_storage_.bound_rows[type];
    }
#ifndef NDEBUG
    auto check_live_entity_membership() const -> bool;
#endif
    auto get_cell_entities(CellCoord const cell_coord) const -> std::span<EntityUniqueId const> {
        auto const dimensions{geometry_.dimensions};
        assert(cell_coord.x >= 0 && cell_coord.x < dimensions.x && cell_coord.y >= 0 &&
               cell_coord.y < dimensions.y && cell_coord.z >= 0 && cell_coord.z < dimensions.z);

        auto const row_stride{dimensions.x};
        auto const plane_stride{row_stride * dimensions.y};
        auto const cell_index{cell_coord.x + cell_coord.y * row_stride +
                              cell_coord.z * plane_stride};
        auto const element{static_cast<std::size_t>(cell_index)};
        auto const count{entity_storage_.cell_counts[element]};
        if (count == 0) {
            return {};
        }

        return std::span{entity_storage_.entities}.subspan(
            static_cast<std::size_t>(entity_storage_.cell_offsets[element]), count);
    }

    /* **************************************** */
    // Spatial queries
    /* **************************************** */
    // Replace output with unique grid members in ascending ID order. Cells must be in bounds;
    // repeated cells are allowed. Membership reflects the last grid rebuild, not current health.
    void collect_unique_entities_in_cells(std::span<CellCoord const> cells,
                                          ml::FrameArray<EntityUniqueId>& out_entities) const;

    struct OverlapCounts {
        std::uint32_t entities{};
        std::uint32_t static_geometry{};
    };

    // Count multi-cell duplicates so the caller can allocate exact output spans before writing.
    auto count_overlaps(WorldAABB const& query_bounds, EntityUniqueId ignored_entity) const
        -> OverlapCounts;
    // Keep the grid and entity state unchanged between counting and writing.
    void write_overlaps(WorldAABB const& query_bounds,
                        EntityUniqueId ignored_entity,
                        std::span<EntityUniqueId> out_entities,
                        std::span<StaticGeometryIndex> out_static_geometry_indices) const;
    void trace_aabbs(LineTraceBatch const& traces, TraceHits::View const& hits) const;
    void trace_aabbs(LineTraceBatch const& traces,
                     TraceHits::View const& hits,
                     std::span<EntityUniqueId const> ignored_entities) const;
    void sweep_aabbs(LineTraceBatch const& centre_paths,
                     Vector3f moving_half_extent,
                     TraceHits::View const& hits,
                     std::span<EntityUniqueId const> ignored_entities = {},
                     TraceEntityFilter entity_filter = TraceEntityFilter::None) const;
  private:
    /* **************************************** */
    // Static-grid building
    /* **************************************** */
    void rebuild_static_grid();

    /* **************************************** */
    // Tracing
    /* **************************************** */
    template <bool Write>
    auto overlaps_impl(WorldAABB const& query_bounds,
                       EntityUniqueId ignored_entity,
                       std::span<EntityUniqueId> out_entities,
                       std::span<StaticGeometryIndex> out_static_geometry_indices) const
        -> OverlapCounts;

    enum class TraceKind : std::uint8_t {
        Line,
        Sweep,
    };

    enum class IgnoredEntityMode : std::uint8_t {
        None,
        PerTrace,
    };

    template <TraceKind Kind, IgnoredEntityMode IgnoredMode, TraceEntityFilter EntityFilter>
    void trace_aabbs_impl(LineTraceBatch traces,
                          TraceHits::View hits,
                          std::span<EntityUniqueId const> ignored_entities,
                          Vector3f moving_half_extent) const;

    /* **************************************** */
    // State
    /* **************************************** */
    GridGeometry geometry_{};
    CollisionGridEntityStorage entity_storage_;
    CollisionGridStaticStorage static_storage_;
};
}
