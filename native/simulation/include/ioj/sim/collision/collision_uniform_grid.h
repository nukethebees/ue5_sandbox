#pragma once

#include <ioj/sim/line_traces.h>
#include <string>

#include <ioj/sim/collision_grid.h>
#include <ioj/sim/collision_grid_entity_storage.h>
#include <ioj/sim/collision_grid_static_storage.h>
#include <ioj/sim/collision_types.h>
#include <ioj/sim/entity_world_bounds.h>
#include <ioj/sim/trace_hits.h>
#include <sandbox/core/frame_array.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ioj::sim {
class AgentAccessor;
}

namespace ioj::sim::collision {
enum class TraceEntityFilter : std::uint8_t {
    None,
    ExcludeFighters,
};

struct CollisionUniformGrid {
    /* **************************************** */
    // Construction and lifecycle
    /* **************************************** */
    explicit CollisionUniformGrid(AgentAccessor const& agents) noexcept;
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
    void rebuild_entity_grid(EntityAABBs const& entity_aabbs);
    auto get_entity_world_bounds() const -> WorldAABBsColumnsConstView;
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
    // Appends exact overlaps. Multi-cell participants may be appended more than once.
    void append_overlaps(WorldAABB const& query_bounds,
                         EntityUniqueId ignored_entity,
                         ml::FrameArray<EntityUniqueId>& out_entities,
                         ml::FrameArray<StaticGeometryIndex>& out_static_geometry_indices) const;
    void trace_aabbs(LineTracesConstView const& traces, TraceHitsView const& hits) const;
    void trace_aabbs(LineTracesConstView const& traces,
                     TraceHitsView const& hits,
                     std::span<EntityUniqueId const> ignored_entities) const;
    void sweep_aabbs(LineTracesConstView const& centre_paths,
                     Vector3f moving_half_extent,
                     TraceHitsView const& hits,
                     std::span<EntityUniqueId const> ignored_entities = {},
                     TraceEntityFilter entity_filter = TraceEntityFilter::None) const;
  private:
    /* **************************************** */
    // Static-grid building
    /* **************************************** */
    void rebuild_static_grid();

    /* **************************************** */
    // State
    /* **************************************** */
    AgentAccessor const& agents_;
    GridGeometry geometry_{};
    CollisionGridEntityStorage entity_storage_;
    CollisionGridStaticStorage static_storage_;
};
}
