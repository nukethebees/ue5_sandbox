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
using DetectedOverlapsView = ioj::sim::collision::DetectedOverlapsView;
using AABBOverlapEventBatch = ioj::sim::collision::AABBOverlapEventBatch;
using AABBOverlapEventBatchView = ioj::sim::collision::AABBOverlapEventBatchView;
using AABBOverlapEventsView = ioj::sim::collision::AABBOverlapEventsView;

struct CollisionSystem {
  public:
    explicit CollisionSystem(EntityRegistry const& registry) noexcept;
    CollisionSystem(CollisionSystem const&) = delete;
    CollisionSystem(CollisionSystem&&) = delete;
    auto operator=(CollisionSystem const&) -> CollisionSystem& = delete;
    auto operator=(CollisionSystem&&) -> CollisionSystem& = delete;

    void initialise(ioj::sim::collision::EntityAABBs const& bounds);
    auto update(std::span<RegistryEntityHandle const> collision_dirty_entities,
                ioj::sim::SimTick tick) -> DetectedOverlapsView;

    void reset_frame_events();
    auto get_aabb_overlap_events() const -> AABBOverlapEventsView {
        return overlap_event_storage_.get_view();
    }

    auto get_entity_aabbs() const noexcept -> ioj::sim::collision::EntityAABBs const& {
        return entity_aabbs_;
    }
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
        std::span<RegistryEntityHandle const> collision_dirty_entities);

    EntityRegistry const& entity_registry_;
    CollisionUniformGrid uniform_grid_;

    ioj::sim::collision::EntityAABBs entity_aabbs_{};
    ioj::sim::collision::CollisionOverlapStorage overlap_storage_;

    ioj::sim::collision::AABBOverlapEventStorage overlap_event_storage_;

    std::vector<RegistryEntityHandle> overlapping_entities_scratch_;
    std::vector<std::int32_t> overlapping_static_geometry_indices_scratch_;
};
}
