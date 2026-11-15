#pragma once

#include <ioj/sim/collision/collision_events.h>
#include <ioj/sim/collision/collision_uniform_grid.h>
#include <ioj/sim/collision/entity_overlaps.h>
#include <ioj/sim/collision/line_traces.h>
#include <ioj/sim/entities/entity_id.h>

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class EntityLookupTables;
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
    explicit CollisionSystem(std::pmr::memory_resource* resource) noexcept;
    CollisionSystem(CollisionSystem const&) = delete;
    CollisionSystem(CollisionSystem&&) = delete;
    auto operator=(CollisionSystem const&) -> CollisionSystem& = delete;
    auto operator=(CollisionSystem&&) -> CollisionSystem& = delete;

    void initialise(CollisionGridSpec grid_spec, EntityAABBs const& bounds);
    void set_static_collision(WorldAABBs bounds);
    auto add_static_collision_aabb(Vector3f min_point, Vector3f max_point) -> StaticGeometryIndex;

    /* **************************************** */
    // Spatial-index lifecycle
    /* **************************************** */
    void refresh_spatial_index(LevelReadAccess const& level);

    /* **************************************** */
    // Overlap detection
    /* **************************************** */
    auto detect_overlaps(EntityLookupTables const& lookups,
                         std::span<EntityId const> overlap_candidates,
                         FrameMemoryResource* const scratch_resource) -> DetectedOverlapsView;
    void collect_overlaps_for_candidates(EntityLookupTables const& lookups,
                                         std::span<EntityId const> overlap_candidates,
                                         EntityEntityOverlaps& entity_overlaps,
                                         EntityStaticOverlaps& static_overlaps,
                                         FrameMemoryResource* const scratch_resource);
    void finalize_overlaps(EntityEntityOverlaps& entity_overlaps,
                           EntityStaticOverlaps& static_overlaps,
                           FrameMemoryResource* const scratch_resource);

    /* **************************************** */
    // Events and bounds
    /* **************************************** */
    void append_overlap_batch(EntityEntityOverlaps::CompactConstView entity_entity_overlaps,
                              EntityStaticOverlaps::CompactConstView entity_static_overlaps);
    void reset_frame_collision_events();
    auto aabb_overlap_events() const -> AABBOverlapEventsView {
        return {entity_entity_overlaps_.const_view(),
                entity_static_overlaps_.const_view(),
                overlap_batches_};
    }
    auto entity_collision_bounds() const -> EntityCellData::CompactConstView;
    auto static_collision_bounds() const -> WorldAABBs::CompactConstView;

    /* **************************************** */
    // State
    /* **************************************** */
    CollisionUniformGrid uniform_grid_;

    EntityAABBs entity_aabbs_{};
    EntityEntityOverlaps entity_entity_overlaps_;
    EntityStaticOverlaps entity_static_overlaps_;
    std::pmr::vector<AABBOverlapEventBatch> overlap_batches_;
};
}
