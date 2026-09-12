#pragma once

#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/simulation/collision_uniform_grid.h>
#include <SpaceGameSimulation/simulation/EntityAABBs.h>
#include <SpaceGameSimulation/simulation/EntityOverlaps.h>
#include <SpaceGameSimulation/support/IndexSpan.h>

struct FTestEntityRegistry;

namespace ml::ioj {
struct FDetectedOverlapsView {
    FEntityEntityOverlaps::ConstView entity_entity_overlaps;
    FEntityStaticOverlaps::ConstView entity_static_overlaps;
};

struct FAABBOverlapEventBatch {
    uint64 tick{};
    FIndexSpan entity_entity_overlaps;
    FIndexSpan entity_static_overlaps;
};

struct FAABBOverlapEventBatchView {
    uint64 tick{};
    FDetectedOverlapsView overlaps;
};

struct FAABBOverlapEventsView {
    FEntityEntityOverlaps::ConstView entity_entity_overlaps;
    FEntityStaticOverlaps::ConstView entity_static_overlaps;
    TConstArrayView<FAABBOverlapEventBatch> batches;

    auto get_batch(int32 const index) const -> FAABBOverlapEventBatchView {
        auto const batch{batches[index]};
        return {
            .tick = batch.tick,
            .overlaps =
                {
                    .entity_entity_overlaps = entity_entity_overlaps.slice(
                        batch.entity_entity_overlaps.offset, batch.entity_entity_overlaps.count),
                    .entity_static_overlaps = entity_static_overlaps.slice(
                        batch.entity_static_overlaps.offset, batch.entity_static_overlaps.count),
                },
        };
    }
};

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
    TArray<FAABBOverlapEventBatch> overlap_event_batches_;

    TArray<FRegistryEntityHandle> overlapping_entities_scratch_;
    TArray<int32> overlapping_static_geometry_indices_scratch_;
    TArray<int32> overlap_sort_indices_scratch_;
};
}
