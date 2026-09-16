#pragma once

#include <cstdint>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/line_traces.h>
#include <ioj/sim/sim_tick.h>
#include <span>

#include <ioj/sim/collision_events.h>
#include <ioj/sim/collision_overlap_storage.h>

#include <ioj/sim/collision/collision_uniform_grid.h>
#include <ioj/sim/entity_overlaps.h>

#include <vector>

namespace ioj::sim {
struct EntityRegistry;
}

namespace ioj::sim::collision {

struct CollisionSystem {
  public:
    CollisionSystem(EntityRegistry const& registry, AgentAccessor const& agents) noexcept;
    CollisionSystem(CollisionSystem const&) = delete;
    CollisionSystem(CollisionSystem&&) = delete;
    auto operator=(CollisionSystem const&) -> CollisionSystem& = delete;
    auto operator=(CollisionSystem&&) -> CollisionSystem& = delete;

    void initialise(EntityAABBs const& bounds);
    auto update(std::span<EntityUniqueId const> collision_dirty_entities, SimTick tick)
        -> DetectedOverlapsView;

    void reset_frame_events();
    void refresh_queries();
    auto get_aabb_overlap_events() const -> AABBOverlapEventsView {
        return overlap_event_storage_.get_view();
    }

    auto get_entity_aabbs() const noexcept -> EntityAABBs const& { return entity_aabbs_; }
    auto get_entity_entity_overlaps() const -> EntityEntityOverlaps::ConstView {
        return overlap_storage_.entity_entity_overlaps();
    }
    auto get_entity_static_overlaps() const -> EntityStaticOverlaps::ConstView {
        return overlap_storage_.entity_static_overlaps();
    }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }
  private:
    void rebuild_grid();
    void collect_overlaps_for_moved_entities(
        std::span<EntityUniqueId const> collision_dirty_entities);

    AgentAccessor const& agents_;
    CollisionUniformGrid uniform_grid_;

    EntityAABBs entity_aabbs_{};
    CollisionOverlapStorage overlap_storage_;

    AABBOverlapEventStorage overlap_event_storage_;

    std::vector<EntityUniqueId> overlapping_entities_scratch_;
    std::vector<std::int32_t> overlapping_static_geometry_indices_scratch_;
};
}
