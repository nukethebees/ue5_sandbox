#pragma once

#include <ioj/sim/line_traces.h>
#include <string>

#include <ioj/sim/collision_grid.h>
#include <ioj/sim/collision_grid_entity_storage.h>
#include <ioj/sim/collision_grid_static_storage.h>
#include <ioj/sim/entity_world_bounds.h>
#include <ioj/sim/trace_hits.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct EntityRegistry;
class AgentAccessor;
}

namespace ioj::sim::collision {
enum class TraceEntityFilter : std::uint8_t {
    None,
    ExcludeFighters,
};

struct CollisionUniformGrid {
    static inline Vector3f const origin{};

    CollisionUniformGrid(EntityRegistry const& entity_registry,
                         AgentAccessor const& agents) noexcept;
    CollisionUniformGrid(CollisionUniformGrid const&) = delete;
    CollisionUniformGrid(CollisionUniformGrid&&) = delete;
    auto operator=(CollisionUniformGrid const&) -> CollisionUniformGrid& = delete;
    auto operator=(CollisionUniformGrid&&) -> CollisionUniformGrid& = delete;

    auto is_configured() const noexcept -> bool;

    auto get_grid_dims() const noexcept -> CellCoord;
    void set_grid_dims(CellCoord const grid_dims) noexcept;

    auto get_cell_dims() const noexcept -> Vector3f;
    void set_cell_dims(Vector3f const cell_dims) noexcept;

    auto num_cells() const -> std::int32_t;
    auto get_non_empty_cell_count() const noexcept -> std::int32_t {
        return entity_storage_.non_empty_cell_count();
    }
    auto get_native_geometry() const noexcept -> GridGeometry { return geometry_; }
    auto get_native_entity_storage() const noexcept -> CollisionGridEntityStorage const& {
        return entity_storage_;
    }
    auto get_cell_entities(CellCoord const cell_coord) const
        -> std::span<RegistryEntityHandle const>;

    auto to_cell_coord(Vector3f pos) const -> CellCoord;
    auto to_min_cell_coord(Vector3f pos) const -> CellCoord;
    auto to_max_cell_coord(Vector3f pos) const -> CellCoord;
    auto to_cell_coord_bounds(Vector3f min_point, Vector3f max_point) const -> CellCoordBounds;

    auto to_cell_min_x(std::int32_t x) const -> float;
    auto to_cell_min_y(std::int32_t y) const -> float;
    auto to_cell_min_z(std::int32_t z) const -> float;
    auto to_cell_min(std::int32_t x, std::int32_t y, std::int32_t z) const -> Vector3f;
    auto to_cell_min(CellCoord coord) const -> Vector3f;

    auto to_cell_centre_x(std::int32_t x) const -> float;
    auto to_cell_centre_y(std::int32_t y) const -> float;
    auto to_cell_centre_z(std::int32_t z) const -> float;
    auto to_cell_centre(std::int32_t x, std::int32_t y, std::int32_t z) const -> Vector3f;
    auto to_cell_centre(CellCoord coord) const -> Vector3f;

    auto is_cell_coord_in_bounds(CellCoord coord) const -> bool;
    auto is_cell_coord_in_bounds(CellCoord min_coord, CellCoord max_coord) const -> bool;
    void are_spheres_in_bounds(Vectors3fConstView centres,
                               float radius,
                               std::span<std::uint8_t> out_results) const;
    static auto to_string(CellCoord value) -> std::string;

    void reset();
    void set_static_aabbs(WorldAABBs static_aabbs);
    auto add_static_aabb(Vector3f min_point, Vector3f max_point) -> std::int32_t;
    void rebuild_grid(EntityAABBs const& entity_aabbs);

    auto get_static_aabbs() const noexcept -> WorldAABBs const& { return static_storage_.aabbs(); }
    auto get_entity_world_bounds() const -> WorldAABBsColumnsConstView {
        return entity_storage_.entity_world_bounds();
    }

    // Appends exact overlaps. Multi-cell participants may be appended more than once.
    void append_overlaps(WorldAABB const& query_bounds,
                         EntityUniqueId ignored_entity,
                         std::vector<EntityUniqueId>& out_entities,
                         std::vector<std::int32_t>& out_static_geometry_indices) const;
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
    auto to_cell_x(float value) const -> std::int32_t;
    auto to_cell_y(float value) const -> std::int32_t;
    auto to_cell_z(float value) const -> std::int32_t;
    auto to_index(std::int32_t x, std::int32_t y, std::int32_t z) const -> std::int32_t;
    auto to_index(CellCoord coord) const -> std::int32_t;
    auto to_index(Vector3f pos) const -> std::int32_t;
    void rebuild_static_grid();

    EntityRegistry const& entity_registry_;
    AgentAccessor const& agents_;

    GridGeometry geometry_{};

    CollisionGridEntityStorage entity_storage_;

    CollisionGridStaticStorage static_storage_;
};
}
