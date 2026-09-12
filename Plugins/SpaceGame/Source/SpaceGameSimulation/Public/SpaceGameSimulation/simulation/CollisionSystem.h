#pragma once

#include <sandbox/simulation/collision_events.h>

#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/collision_uniform_grid.h>
#include <SpaceGameSimulation/simulation/EntityAABBs.h>
#include <SpaceGameSimulation/simulation/EntityOverlaps.h>

#include <vector>

struct FTestEntityRegistry;

namespace ml::ioj {
using FDetectedOverlapsView = simulation::collision::DetectedOverlapsView;
using FAABBOverlapEventBatch = simulation::collision::AABBOverlapEventBatch;
using FAABBOverlapEventBatchView = simulation::collision::AABBOverlapEventBatchView;
using FAABBOverlapEventsView = simulation::collision::AABBOverlapEventsView;

struct SPACEGAMESIMULATION_API FCollisionSystem {
  public:
    explicit FCollisionSystem(FTestEntityRegistry const& registry) noexcept;
    FCollisionSystem(FCollisionSystem const&) = delete;
    FCollisionSystem(FCollisionSystem&&) = delete;
    auto operator=(FCollisionSystem const&) -> FCollisionSystem& = delete;
    auto operator=(FCollisionSystem&&) -> FCollisionSystem& = delete;

    void initialise(FEntityAABBs const& bounds);
    auto update(TConstArrayView<FRegistryEntityHandle> collision_dirty_entities, uint64 tick)
        -> FDetectedOverlapsView;

    void reset_frame_events();
    auto get_aabb_overlap_events() const -> FAABBOverlapEventsView {
        return {entity_entity_overlap_events_.get_const_view(),
                entity_static_overlap_events_.get_const_view(),
                overlap_event_batches_};
    }

    auto get_entity_aabbs() const noexcept -> FEntityAABBs const& { return entity_aabbs_; }
    auto get_entity_entity_overlaps() const -> FEntityEntityOverlaps::ConstView {
        return entity_entity_overlaps_.get_const_view();
    }
    auto get_entity_static_overlaps() const -> FEntityStaticOverlaps::ConstView {
        return entity_static_overlaps_.get_const_view();
    }
    auto get_uniform_grid() noexcept -> CollisionUniformGrid& { return uniform_grid_; }
    auto get_uniform_grid() const noexcept -> CollisionUniformGrid const& { return uniform_grid_; }
  private:
    void rebuild_grid();
    void collect_overlaps_for_moved_entities(
        TConstArrayView<FRegistryEntityHandle> collision_dirty_entities);
    void sort_and_deduplicate_overlaps();

    FTestEntityRegistry const& entity_registry_;
    CollisionUniformGrid uniform_grid_;

    FEntityAABBs entity_aabbs_{};
    FEntityEntityOverlaps entity_entity_overlaps_;
    FEntityStaticOverlaps entity_static_overlaps_;

    FEntityEntityOverlaps entity_entity_overlap_events_;
    FEntityStaticOverlaps entity_static_overlap_events_;
    std::vector<FAABBOverlapEventBatch> overlap_event_batches_;

    std::vector<FRegistryEntityHandle> overlapping_entities_scratch_;
    std::vector<int32> overlapping_static_geometry_indices_scratch_;
    std::vector<int32> overlap_sort_indices_scratch_;
};
}
