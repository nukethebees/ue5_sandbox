#pragma once
#include <ioj/levels/team.h>
#include <ioj/sim/collision/collision_system.h>
#include <ioj/sim/collision/line_traces.h>
#include <ioj/sim/collision/query_result_types.h>
#include <ioj/sim/collision/trace_hits.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_instance_handle.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/entities/entity_type_radii.h>
#include <ioj/sim/player/player_spatial_data.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace ioj::sim {
struct EntityTables;
class LevelReadAccess;
struct RangeQueryResults;
struct SpatialQueryManager;
struct SpatialQueryManagerTestAccess;
}

namespace ioj {
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
    void trace_line_of_sight(Vectors3fResolvedConstView start_locations,
                             Vectors3fResolvedConstView end_locations,
                             std::span<EntityId> out_entity_ids,
                             FrameMemoryResource* scratch_resource) const;
    void has_line_of_sight_to_targets(Vector3f const& start_location,
                                      Vectors3fResolvedConstView end_locations,
                                      std::span<EntityId const> targets,
                                      std::span<LineQueryResult> has_los,
                                      FrameMemoryResource* scratch_resource) const;
    void have_clear_lines(Vectors3fResolvedConstView start_locations,
                          Vectors3fResolvedConstView end_locations,
                          std::span<LineQueryResult> clear_lines,
                          FrameMemoryResource* scratch_resource,
                          std::span<EntityId const> ignored_entities = {}) const;
    void trace_closest_lines(Vectors3fResolvedConstView start_locations,
                             Vectors3fResolvedConstView end_locations,
                             TraceHits::View out_hits,
                             std::span<EntityId const> ignored_entities = {}) const;
    void sweep_closest_aabbs(
        Vectors3fResolvedConstView start_locations,
        Vectors3fResolvedConstView end_locations,
        Vector3f moving_half_extent,
        TraceHits::View out_hits,
        std::span<EntityId const> ignored_entities = {},
        collision::TraceEntityFilter entity_filter = collision::TraceEntityFilter::None) const;

    /* **************************************** */
    // Batched range queries
    /* **************************************** */
    // Requires a current, live grid and stable entity state, as guaranteed during Thinking.
    // Replace caller-owned results in request order; candidate order within each range is
    // unspecified. Range tests use inclusive centre distance and abs(radius). Directions point from
    // each origin to its candidate, with zero for squared distances below 1.e-8.
    void collect_non_team_entities_in_range(Vectors3fResolvedConstView origins,
                                            std::span<Team const> teams,
                                            float radius,
                                            RangeQueryResults& out_results,
                                            FrameMemoryResource* const scratch_resource) const;

    /* **************************************** */
    // Scalar and entity queries
    /* **************************************** */
    // Return unique membership from the last grid rebuild, ordered by entity ID.
    void collect_unique_entities_in_cells(std::span<collision::CellCoord const> cells,
                                          FrameArray<EntityId>& out_entities) const;

    auto has_clear_line(Vector3f start_location,
                        Vector3f end_location,
                        FrameMemoryResource* scratch_resource,
                        EntityId ignored_entity = {}) const -> bool;
    auto trace_closest(Vector3f start_location,
                       Vector3f end_location,
                       FrameMemoryResource* scratch_resource,
                       EntityId ignored_entity = {}) const -> LineTraceResult;

    auto collect_non_team_entities_in_range(Vector3f const& origin,
                                            Team const team,
                                            float const radius,
                                            std::span<EntityId> const out_entities,
                                            FrameMemoryResource* scratch_resource) const
        -> std::uint32_t;
    auto collect_entities_of_type_in_range(Vector3f const& origin,
                                           EntityType entity_type,
                                           float radius,
                                           EntityId ignored_entity,
                                           std::span<EntityId> out_entities,
                                           FrameMemoryResource* scratch_resource) const
        -> std::uint32_t;
    auto get_any_non_team_entity(Team const team, EntityType const entity_type) const -> EntityId;
    void are_spheres_in_bounds(Vectors3fResolvedConstView centres,
                               float radius,
                               std::span<collision::SphereInBoundsResult> out_results) const;
    auto get_entity_type_radius(EntityType entity_type) const noexcept -> float;
    auto get_entity_type_radii() const noexcept -> EntityTypeRadii const&;
    void copy_entity_locations(std::span<EntityId const> ids,
                               Vectors3fResolvedView output,
                               FrameMemoryResource* scratch_resource) const;
    void copy_entity_motion(std::span<EntityId const> ids,
                            Vectors3fResolvedView locations,
                            Vectors3fResolvedView velocities,
                            std::span<EntityInstanceHandle> handles,
                            FrameMemoryResource* scratch_resource) const;

    /* **************************************** */
    // Collision and spatial-index lifecycle
    /* **************************************** */
    void set_static_collision(collision::WorldAABBs bounds);
    auto add_static_collision_aabb(Vector3f min_point, Vector3f max_point)
        -> collision::StaticGeometryIndex;
    void refresh_spatial_index(LevelReadAccess const& level);
    auto detect_overlaps(std::span<EntityId const> overlap_candidates,
                         FrameMemoryResource* const scratch_resource)
        -> collision::DetectedOverlapsView;
    void reset_frame_collision_events();
    auto get_aabb_overlap_events() const -> collision::AABBOverlapEventsView;
    // Debug snapshot of the last rebuild; may include entities removed later in Resolution.
    auto get_entity_collision_bounds() const -> collision::EntityCellData::ConstView;
    auto get_static_collision_bounds() const -> collision::WorldAABBs::ConstView;
  private:
    friend struct SpatialQueryManagerTestAccess;

    /* **************************************** */
    // State
    /* **************************************** */
    EntityTables const& entity_tables_;

    collision::CollisionSystem collision_system_;
    EntityTypeRadii entity_radii_{};
    EnumArray<EntityType, Vectors3fResolvedConstView> locations_;
    EnumArray<EntityType, Vectors3fResolvedConstView> velocities_;
    PlayerSpatialData player_spatial_;
};
}
