#pragma once

#include <sandbox/simulation/line_traces.h>
#include <string>

#include <sandbox/simulation/collision_grid.h>
#include <sandbox/simulation/collision_grid_entity_storage.h>
#include <sandbox/simulation/collision_grid_queries.h>
#include <sandbox/simulation/collision_grid_static_storage.h>
#include <sandbox/simulation/entity_world_bounds.h>
#include <sandbox/simulation/simulation/TraceHits.h>
#include <sandbox/simulation/spatial_query_telemetry.h>

#include <cstdint>
#include <span>
#include <vector>

struct FTestEntityRegistry;

namespace ml::ioj {
using ETraceEntityFilter = simulation::collision::TraceEntityFilter;

using FCellCoordBounds = simulation::collision::CellCoordBounds;

using FCollisionGridTelemetrySnapshot = simulation::collision::CollisionGridTelemetrySnapshot;

struct CollisionUniformGrid {
    static inline simulation::Vector3f const origin{};

    explicit CollisionUniformGrid(FTestEntityRegistry const& entity_registry) noexcept;
    CollisionUniformGrid(CollisionUniformGrid const&) = delete;
    CollisionUniformGrid(CollisionUniformGrid&&) = delete;
    auto operator=(CollisionUniformGrid const&) -> CollisionUniformGrid& = delete;
    auto operator=(CollisionUniformGrid&&) -> CollisionUniformGrid& = delete;

    auto is_configured() const noexcept -> bool;

    auto get_grid_dims() const noexcept -> simulation::collision::CellCoord;
    void set_grid_dims(simulation::collision::CellCoord const grid_dims) noexcept;

    auto get_cell_dims() const noexcept -> simulation::Vector3f;
    void set_cell_dims(simulation::Vector3f const cell_dims) noexcept;

    auto num_cells() const -> std::int32_t;
    auto get_non_empty_cell_count() const noexcept -> std::int32_t {
        return entity_storage_.non_empty_cell_count();
    }
    auto get_native_geometry() const noexcept -> simulation::collision::GridGeometry {
        return geometry_;
    }
    auto get_native_entity_storage() const noexcept
        -> simulation::collision::CollisionGridEntityStorage const& {
        return entity_storage_;
    }
    auto get_cell_entities(simulation::collision::CellCoord const cell_coord) const
        -> std::span<FRegistryEntityHandle const>;

    auto to_cell_coord(simulation::Vector3f pos) const -> simulation::collision::CellCoord;
    auto to_min_cell_coord(simulation::Vector3f pos) const -> simulation::collision::CellCoord;
    auto to_max_cell_coord(simulation::Vector3f pos) const -> simulation::collision::CellCoord;
    auto to_cell_coord_bounds(simulation::Vector3f min_point, simulation::Vector3f max_point) const
        -> FCellCoordBounds;

    auto to_cell_min_x(std::int32_t x) const -> float;
    auto to_cell_min_y(std::int32_t y) const -> float;
    auto to_cell_min_z(std::int32_t z) const -> float;
    auto to_cell_min(std::int32_t x, std::int32_t y, std::int32_t z) const -> simulation::Vector3f;
    auto to_cell_min(simulation::collision::CellCoord coord) const -> simulation::Vector3f;

    auto to_cell_centre_x(std::int32_t x) const -> float;
    auto to_cell_centre_y(std::int32_t y) const -> float;
    auto to_cell_centre_z(std::int32_t z) const -> float;
    auto to_cell_centre(std::int32_t x, std::int32_t y, std::int32_t z) const
        -> simulation::Vector3f;
    auto to_cell_centre(simulation::collision::CellCoord coord) const -> simulation::Vector3f;

    auto is_cell_coord_in_bounds(simulation::collision::CellCoord coord) const -> bool;
    auto is_cell_coord_in_bounds(simulation::collision::CellCoord min_coord,
                                 simulation::collision::CellCoord max_coord) const -> bool;
    void are_spheres_in_bounds(simulation::Vectors3fConstView centres,
                               float radius,
                               std::span<std::uint8_t> out_results) const;
    static auto to_string(simulation::collision::CellCoord value) -> std::string;

    void reset();
    void set_static_aabbs(simulation::collision::WorldAABBs static_aabbs);
    auto add_static_aabb(simulation::Vector3f min_point, simulation::Vector3f max_point)
        -> std::int32_t;
    void rebuild_grid(simulation::collision::EntityAABBs const& entity_aabbs);
    void reset_runtime_telemetry() noexcept;
    auto get_runtime_telemetry() const noexcept -> FCollisionGridTelemetrySnapshot;

    auto get_static_aabbs() const noexcept -> simulation::collision::WorldAABBs const& {
        return static_storage_.aabbs();
    }
    auto get_entity_world_bounds() const -> simulation::collision::WorldAABBsColumnsConstView {
        return entity_storage_.entity_world_bounds();
    }

    // Appends exact overlaps. Multi-cell participants may be appended more than once.
    void append_overlaps(simulation::collision::WorldAABB const& query_bounds,
                         FRegistryEntityHandle ignored_entity,
                         std::vector<FRegistryEntityHandle>& out_entities,
                         std::vector<std::int32_t>& out_static_geometry_indices) const;
    void trace_aabbs(simulation::LineTracesConstView const& traces,
                     FTraceHitsView const& hits) const;
    void trace_aabbs(simulation::LineTracesConstView const& traces,
                     FTraceHitsView const& hits,
                     std::span<FRegistryEntityHandle const> ignored_entities) const;
    void sweep_aabbs(simulation::LineTracesConstView const& centre_paths,
                     simulation::Vector3f moving_half_extent,
                     FTraceHitsView const& hits,
                     std::span<FRegistryEntityHandle const> ignored_entities = {},
                     ETraceEntityFilter entity_filter = ETraceEntityFilter::None) const;
  private:
    auto to_cell_x(float value) const -> std::int32_t;
    auto to_cell_y(float value) const -> std::int32_t;
    auto to_cell_z(float value) const -> std::int32_t;
    auto to_index(std::int32_t x, std::int32_t y, std::int32_t z) const -> std::int32_t;
    auto to_index(simulation::collision::CellCoord coord) const -> std::int32_t;
    auto to_index(simulation::Vector3f pos) const -> std::int32_t;
    void rebuild_static_grid();

    FTestEntityRegistry const& entity_registry_;

    simulation::collision::GridGeometry geometry_{};

    simulation::collision::CollisionGridEntityStorage entity_storage_;

    simulation::collision::CollisionGridStaticStorage static_storage_;

    simulation::collision::CollisionGridTelemetry telemetry_;
};
}
