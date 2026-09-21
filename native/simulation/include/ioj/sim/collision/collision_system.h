#pragma once

#include <cstdint>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/line_traces.h>
#include <span>

#include <ioj/sim/collision_events.h>

#include <ioj/sim/collision/collision_uniform_grid.h>
#include <ioj/sim/entity_overlaps.h>

namespace ml {
class FrameScratch;
}

namespace ioj::sim {
class AgentAccessor;
struct SpatialQueryManager;
struct SpatialQueryManagerTestAccess;
}

namespace ioj::sim::collision {

class CollisionSystem {
  private:
    friend struct ::ioj::sim::SpatialQueryManager;
    friend struct ::ioj::sim::SpatialQueryManagerTestAccess;

    explicit CollisionSystem(AgentAccessor const& agents) noexcept;
    CollisionSystem(CollisionSystem const&) = delete;
    CollisionSystem(CollisionSystem&&) = delete;
    auto operator=(CollisionSystem const&) -> CollisionSystem& = delete;
    auto operator=(CollisionSystem&&) -> CollisionSystem& = delete;

    void initialise(GridGeometry grid_geometry, EntityAABBs const& bounds);
    void set_static_collision(WorldAABBs bounds);
    auto add_static_collision_aabb(Vector3f min_point, Vector3f max_point) -> StaticGeometryIndex;

    void refresh_spatial_index();
    auto detect_overlaps(std::span<EntityUniqueId const> overlap_candidates,
                         ml::FrameScratch& scratch) -> DetectedOverlapsView;

    void reset_frame_collision_events();
    auto get_aabb_overlap_events() const -> AABBOverlapEventsView {
        return overlap_event_storage_.get_view();
    }
    auto get_entity_collision_bounds() const -> WorldAABBsColumnsConstView;
    auto get_static_collision_bounds() const -> WorldAABBsColumnsConstView;

    void collect_overlaps_for_candidates(std::span<EntityUniqueId const> overlap_candidates,
                                         ml::FrameScratch& scratch);
    void finalize_overlaps(ml::FrameScratch& scratch);

    AgentAccessor const& agents_;
    CollisionUniformGrid uniform_grid_;

    EntityAABBs entity_aabbs_{};
    EntityEntityOverlaps entity_entity_overlaps_;
    EntityStaticOverlaps entity_static_overlaps_;

    AABBOverlapEventStorage overlap_event_storage_;
};
}
