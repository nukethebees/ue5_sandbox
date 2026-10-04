#pragma once
#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/entity_instance_handle.h>
#include <ioj/sim/entity_type_radii.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/line_traces.h>
#include <ioj/sim/player_spatial_data.h>
#include <ioj/sim/query_result_types.h>
#include <ioj/sim/trace_hits.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace ioj::sim {
struct EntityTables;
class LevelReadAccess;
struct FrameRangeQueryResults;
struct SpatialQueryManager;
struct SpatialQueryManagerTestAccess;
}

namespace ml {
class FrameMemoryResource;
}

namespace ioj::sim {
// Entity queries require published lookup rows and a current grid: StableSetup, Thinking,
// Action, or pre-compaction Resolution. Neither is valid between ticks.
struct SpatialQueryManager {
  public:
    /* **************************************** */
    // Construction and setup
    /* **************************************** */
    explicit SpatialQueryManager(
        EntityTables const& agents,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource());
    SpatialQueryManager(SpatialQueryManager const&) = delete;
    SpatialQueryManager(SpatialQueryManager&&) = delete;
    auto operator=(SpatialQueryManager const&) -> SpatialQueryManager& = delete;
    auto operator=(SpatialQueryManager&&) -> SpatialQueryManager& = delete;

    void initialise(collision::GridGeometry grid_geometry,
                    collision::EntityAABBs const& entity_bounds);
    auto get_grid_geometry() const noexcept -> collision::GridGeometry {
        auto const& grid{collision_system_.uniform_grid_};
        return {grid.get_grid_dims(), grid.get_cell_dims()};
    }

    /* **************************************** */
    // Batched line queries
    /* **************************************** */
    void trace_line_of_sight(Vectors3fConstView start_locations,
                             Vectors3fConstView end_locations,
                             std::span<EntityUniqueId> out_entity_ids,
                             ml::FrameMemoryResource* scratch_resource) const;
    void has_line_of_sight_to_targets(Vector3f const& start_location,
                                      Vectors3fConstView end_locations,
                                      std::span<EntityUniqueId const> targets,
                                      std::span<LineQueryResult> has_los,
                                      ml::FrameMemoryResource* scratch_resource) const;
    void have_clear_lines(Vectors3fConstView start_locations,
                          Vectors3fConstView end_locations,
                          std::span<LineQueryResult> clear_lines,
                          ml::FrameMemoryResource* scratch_resource,
                          std::span<EntityUniqueId const> ignored_entities = {}) const;
    void trace_closest_lines(Vectors3fConstView start_locations,
                             Vectors3fConstView end_locations,
                             TraceHits::View out_hits,
                             std::span<EntityUniqueId const> ignored_entities = {}) const;
    void sweep_closest_aabbs(
        Vectors3fConstView start_locations,
        Vectors3fConstView end_locations,
        Vector3f moving_half_extent,
        TraceHits::View out_hits,
        std::span<EntityUniqueId const> ignored_entities = {},
        collision::TraceEntityFilter entity_filter = collision::TraceEntityFilter::None) const;

    /* **************************************** */
    // Batched range queries
    /* **************************************** */
    // Requires a current, live grid and stable entity state, as guaranteed during Thinking.
    // Replace caller-owned results in request order; candidate order within each range is
    // unspecified. Range tests use inclusive centre distance and abs(radius). Directions point from
    // each origin to its candidate, with zero for squared distances below 1.e-8.
    void collect_non_team_entities_in_range(Vectors3fConstView origins,
                                            std::span<Team const> teams,
                                            float radius,
                                            FrameRangeQueryResults& out_results,
                                            ml::FrameMemoryResource* const scratch_resource) const;

    /* **************************************** */
    // Scalar and entity queries
    /* **************************************** */
    // Return unique membership from the last grid rebuild, ordered by entity ID.
    void collect_unique_entities_in_cells(std::span<collision::CellCoord const> cells,
                                          ml::FrameArray<EntityUniqueId>& out_entities) const;

    auto has_clear_line(Vector3f start_location,
                        Vector3f end_location,
                        ml::FrameMemoryResource* scratch_resource,
                        EntityUniqueId ignored_entity = {}) const -> bool;
    auto trace_closest(Vector3f start_location,
                       Vector3f end_location,
                       ml::FrameMemoryResource* scratch_resource,
                       EntityUniqueId ignored_entity = {}) const -> LineTraceResult;

    auto collect_non_team_entities_in_range(Vector3f const& origin,
                                            Team const team,
                                            float const radius,
                                            std::span<EntityUniqueId> const out_entities,
                                            ml::FrameMemoryResource* scratch_resource) const
        -> std::uint32_t;
    auto collect_entities_of_type_in_range(Vector3f const& origin,
                                           EntityType entity_type,
                                           float radius,
                                           EntityUniqueId ignored_entity,
                                           std::span<EntityUniqueId> out_entities,
                                           ml::FrameMemoryResource* scratch_resource) const
        -> std::uint32_t;
    auto get_any_non_team_entity(Team const team, EntityType const entity_type) const
        -> EntityUniqueId;
    void are_spheres_in_bounds(Vectors3fConstView centres,
                               float radius,
                               std::span<collision::SphereInBoundsResult> out_results) const;
    auto get_entity_type_radius(EntityType entity_type) const noexcept -> float;
    auto get_entity_type_radii() const noexcept -> EntityTypeRadii const&;
    void copy_entity_locations(std::span<EntityUniqueId const> ids,
                               Vectors3fView locations,
                               ml::FrameMemoryResource* scratch_resource) const;
    void copy_entity_motion(std::span<EntityUniqueId const> ids,
                            Vectors3fView locations,
                            Vectors3fView velocities,
                            std::span<EntityInstanceHandle> handles,
                            ml::FrameMemoryResource* scratch_resource) const;

    /* **************************************** */
    // Collision and spatial-index lifecycle
    /* **************************************** */
    void set_static_collision(collision::WorldAABBs bounds);
    auto add_static_collision_aabb(Vector3f min_point, Vector3f max_point)
        -> collision::StaticGeometryIndex;
    void refresh_spatial_index(LevelReadAccess const& level);
    auto detect_overlaps(std::span<EntityUniqueId const> overlap_candidates,
                         ml::FrameMemoryResource* const scratch_resource)
        -> collision::DetectedOverlapsView;
    void reset_frame_collision_events();
    auto get_aabb_overlap_events() const -> collision::AABBOverlapEventsView;
    // Debug snapshot of the last rebuild; may include entities removed later in Resolution.
    auto get_entity_collision_bounds() const -> collision::EntityCellData::ConstView;
    auto get_static_collision_bounds() const -> collision::WorldAABBs::ConstView;
#ifndef NDEBUG
    auto check_live_grid_membership() const -> bool {
        return collision_system_.uniform_grid_.check_live_entity_membership();
    }
#endif
  private:
    friend struct SpatialQueryManagerTestAccess;

    /* **************************************** */
    // State
    /* **************************************** */
    EntityTables const& entity_tables_;

    collision::CollisionSystem collision_system_;
    EntityTypeRadii entity_radii_{};
    ml::EnumArray<EntityType, Vectors3fConstView> locations_;
    ml::EnumArray<EntityType, Vectors3fConstView> velocities_;
    PlayerSpatialData player_spatial_;
};
}
