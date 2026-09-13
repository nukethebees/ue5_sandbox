#include "SpaceGameSimulation/simulation/CollisionSystem.h"

#include <sandbox/simulation/entity_overlap_operations.h>
#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>

namespace ml::ioj {
void FCollisionSystem::initialise(FEntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
    entity_entity_overlaps_.reset();
    entity_static_overlaps_.reset();
    reset_frame_events();
    overlapping_entities_scratch_.clear();
    overlapping_static_geometry_indices_scratch_.clear();
    overlap_sort_indices_scratch_.clear();
}
auto FCollisionSystem::update(TConstArrayView<FRegistryEntityHandle> const collision_dirty_entities,
                              uint64 const tick) -> FDetectedOverlapsView {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::update);
    rebuild_grid();
    collect_overlaps_for_moved_entities(collision_dirty_entities);

    overlap_event_storage_.append_batch(
        tick, entity_entity_overlaps_.get_const_view(), entity_static_overlaps_.get_const_view());

    return {entity_entity_overlaps_.get_const_view(), entity_static_overlaps_.get_const_view()};
}
void FCollisionSystem::reset_frame_events() {
    overlap_event_storage_.reset();
}
FCollisionSystem::FCollisionSystem(FTestEntityRegistry const& registry) noexcept
    : entity_registry_{registry}
    , uniform_grid_{registry} {}
void FCollisionSystem::rebuild_grid() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::rebuild_grid);
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
void FCollisionSystem::collect_overlaps_for_moved_entities(
    TConstArrayView<FRegistryEntityHandle> const collision_dirty_entities) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::collect_overlaps_for_moved_entities);

    entity_entity_overlaps_.reset();
    entity_static_overlaps_.reset();

    auto const& entity_data{entity_registry_.get_entity_data()};
    for (auto const dirty_entity : collision_dirty_entities) {
        if (!entity_registry_.is_valid_alive(dirty_entity)) {
            continue;
        }

        auto const entity_index{dirty_entity.index};
        auto const entity_type_index{std::to_underlying(entity_data.entity_types[entity_index])};
        auto const bounds{make_entity_world_bounds(
            entity_aabbs_,
            entity_type_index,
            entity_data.locations[entity_index],
            FRotator3f{ml::get_rotator3d(entity_data.rotations, entity_index)})};

        overlapping_entities_scratch_.clear();
        overlapping_static_geometry_indices_scratch_.clear();
        uniform_grid_.append_overlaps(bounds,
                                      dirty_entity,
                                      overlapping_entities_scratch_,
                                      overlapping_static_geometry_indices_scratch_);

        for (auto const overlapping_entity : overlapping_entities_scratch_) {
            if (overlapping_entity < dirty_entity) {
                entity_entity_overlaps_.add(overlapping_entity, dirty_entity);
            } else {
                entity_entity_overlaps_.add(dirty_entity, overlapping_entity);
            }
        }

        for (auto const static_geometry_index : overlapping_static_geometry_indices_scratch_) {
            entity_static_overlaps_.add(dirty_entity, static_geometry_index);
        }
    }

    simulation::collision::sort_and_deduplicate(entity_entity_overlaps_,
                                                overlap_sort_indices_scratch_);
    simulation::collision::sort_and_deduplicate(entity_static_overlaps_,
                                                overlap_sort_indices_scratch_);
}
}
