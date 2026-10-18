#pragma once

#include <ioj/sim/collision/collision_events.h>
#include <ioj/sim/collision/collision_uniform_grid.h>
#include <ioj/sim/collision/entity_overlaps.h>
#include <ioj/sim/collision/line_traces.h>
#include <ioj/sim/entities/entity_unique_id.h>

#include <cstdint>
#include <span>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct EntityTables;
class LevelReadAccess;
struct SpatialQueryManager;
struct SpatialQueryManagerTestAccess;
}

namespace ioj::sim::collision {

class CollisionSystem {
  private:
    friend struct ::ioj::sim::SpatialQueryManager;
    friend struct ::ioj::sim::SpatialQueryManagerTestAccess;

    /* **************************************** */
    // Construction and setup
    /* **************************************** */
    explicit CollisionSystem(EntityTables const& agents,
                             std::pmr::memory_resource* resource) noexcept;
    CollisionSystem(CollisionSystem const&) = delete;
    CollisionSystem(CollisionSystem&&) = delete;
    auto operator=(CollisionSystem const&) -> CollisionSystem& = delete;
    auto operator=(CollisionSystem&&) -> CollisionSystem& = delete;

    void initialise(GridGeometry grid_geometry, EntityAABBs const& bounds);
    void set_static_collision(WorldAABBs bounds);
    auto add_static_collision_aabb(Vector3f min_point, Vector3f max_point) -> StaticGeometryIndex;

    /* **************************************** */
    // Spatial-index lifecycle
    /* **************************************** */
    void refresh_spatial_index(LevelReadAccess const& level);

    /* **************************************** */
    // Overlap detection
    /* **************************************** */
    auto detect_overlaps(std::span<EntityUniqueId const> overlap_candidates,
                         ioj::FrameMemoryResource* const scratch_resource) -> DetectedOverlapsView;
    void collect_overlaps_for_candidates(std::span<EntityUniqueId const> overlap_candidates,
                                         EntityEntityOverlaps& entity_overlaps,
                                         EntityStaticOverlaps& static_overlaps,
                                         ioj::FrameMemoryResource* const scratch_resource);
    void finalize_overlaps(EntityEntityOverlaps& entity_overlaps,
                           EntityStaticOverlaps& static_overlaps,
                           ioj::FrameMemoryResource* const scratch_resource);

    /* **************************************** */
    // Events and bounds
    /* **************************************** */
    void reset_frame_collision_events();
    auto get_aabb_overlap_events() const -> AABBOverlapEventsView {
        return overlap_event_storage_.get_view();
    }
    auto get_entity_collision_bounds() const -> EntityCellData::ConstView;
    auto get_static_collision_bounds() const -> WorldAABBs::ConstView;

    /* **************************************** */
    // State
    /* **************************************** */
    EntityTables const& entity_tables_;
    CollisionUniformGrid uniform_grid_;

    EntityAABBs entity_aabbs_{};
    AABBOverlapEventStorage overlap_event_storage_;
};
}
