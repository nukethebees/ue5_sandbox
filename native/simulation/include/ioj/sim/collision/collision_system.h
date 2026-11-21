#pragma once

#include <ioj/sim/collision/collision_uniform_grid.h>
#include <ioj/sim/collision/entity_overlaps.h>
#include <ioj/sim/collision/line_traces.h>
#include <ioj/sim/entities/entity_id.h>

#include <cstdint>
#include <span>

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
    CollisionSystem() = default;
    CollisionSystem(CollisionSystem const&) = delete;
    CollisionSystem(CollisionSystem&&) = delete;
    auto operator=(CollisionSystem const&) -> CollisionSystem& = delete;
    auto operator=(CollisionSystem&&) -> CollisionSystem& = delete;

    void initialise(CollisionGridSpec grid_spec, EntityAABBs const& bounds);
    void set_static_collision(WorldAABBs bounds);

    /* **************************************** */
    // Spatial-index lifecycle
    /* **************************************** */
    void refresh_spatial_index(LevelReadAccess const& level);

    /* **************************************** */
    // Overlap detection
    /* **************************************** */
    void detect_overlaps(EntityLookupTables const& lookups,
                         std::span<EntityId const> overlap_candidates,
                         EntityEntityOverlaps& entity_overlaps,
                         EntityStaticOverlaps& static_overlaps,
                         FrameMemoryResource* const scratch_resource);
    void finalize_overlaps(EntityEntityOverlaps& entity_overlaps,
                           EntityStaticOverlaps& static_overlaps,
                           FrameMemoryResource* const scratch_resource);

    /* **************************************** */
    // Bounds
    /* **************************************** */
    auto entity_collision_bounds() const -> EntityCellData::CompactConstView;
    auto static_collision_bounds() const -> WorldAABBs::CompactConstView;

    /* **************************************** */
    // State
    /* **************************************** */
    CollisionUniformGrid uniform_grid_;

    EntityAABBs entity_aabbs_{};
};
}
