#pragma once

#include <cstdint>
#include <sandbox/simulation/entity_types.h>
#include <sandbox/simulation/line_traces.h>
#include <span>

#include <sandbox/simulation/collision_events.h>
#include <sandbox/simulation/collision_overlap_storage.h>

#include <sandbox/simulation/simulation/collision_uniform_grid.h>
#include <sandbox/simulation/simulation/EntityOverlaps.h>

#include <vector>

struct FTestEntityRegistry;

namespace ml::ioj {
using FDetectedOverlapsView = simulation::collision::DetectedOverlapsView;
using FAABBOverlapEventBatch = simulation::collision::AABBOverlapEventBatch;
using FAABBOverlapEventBatchView = simulation::collision::AABBOverlapEventBatchView;
using FAABBOverlapEventsView = simulation::collision::AABBOverlapEventsView;

struct FCollisionSystem {
  public:
    explicit FCollisionSystem(FTestEntityRegistry const& registry) noexcept;
    FCollisionSystem(FCollisionSystem const&) = delete;
    FCollisionSystem(FCollisionSystem&&) = delete;
    auto operator=(FCollisionSystem const&) -> FCollisionSystem& = delete;
    auto operator=(FCollisionSystem&&) -> FCollisionSystem& = delete;

    void initialise(simulation::collision::EntityAABBs const& bounds);
    auto update(std::span<FRegistryEntityHandle const> collision_dirty_entities, std::uint64_t tick)
        -> FDetectedOverlapsView;

    void reset_frame_events();
    auto get_aabb_overlap_events() const -> FAABBOverlapEventsView {
        return overlap_event_storage_.get_view();
    }

    auto get_entity_aabbs() const noexcept -> simulation::collision::EntityAABBs const& {
        return entity_aabbs_;
    }
    auto get_entity_entity_overlaps() const -> FEntityEntityOverlaps::ConstView {
        return overlap_storage_.entity_entity_overlaps();
    }
    auto get_entity_static_overlaps() const -> FEntityStaticOverlaps::ConstView {
        return overlap_storage_.entity_static_overlaps();
    }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }
  private:
    void rebuild_grid();
    void collect_overlaps_for_moved_entities(
        std::span<FRegistryEntityHandle const> collision_dirty_entities);

    FTestEntityRegistry const& entity_registry_;
    CollisionUniformGrid uniform_grid_;

    simulation::collision::EntityAABBs entity_aabbs_{};
    simulation::collision::CollisionOverlapStorage overlap_storage_;

    simulation::collision::AABBOverlapEventStorage overlap_event_storage_;

    std::vector<FRegistryEntityHandle> overlapping_entities_scratch_;
    std::vector<std::int32_t> overlapping_static_geometry_indices_scratch_;
};
}
