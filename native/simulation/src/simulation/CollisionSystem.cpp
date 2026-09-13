#include "sandbox/simulation/simulation/CollisionSystem.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <sandbox/core/diagnostics.h>
#include <sandbox/simulation/rotator_math.h>
#include <thread>

#include <sandbox/simulation/entities/TestEntityRegistry.h>

namespace ml::ioj {
void FCollisionSystem::initialise(simulation::collision::EntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
    overlap_storage_.reset();
    reset_frame_events();
    overlapping_entities_scratch_.clear();
    overlapping_static_geometry_indices_scratch_.clear();
}
auto FCollisionSystem::update(std::span<FRegistryEntityHandle const> const collision_dirty_entities,
                              simulation::SimTick const tick) -> FDetectedOverlapsView {
    rebuild_grid();
    collect_overlaps_for_moved_entities(collision_dirty_entities);

    overlap_event_storage_.append_batch(
        tick, overlap_storage_.entity_entity_overlaps(), overlap_storage_.entity_static_overlaps());

    return overlap_storage_.get_view();
}
void FCollisionSystem::reset_frame_events() {
    overlap_event_storage_.reset();
}
FCollisionSystem::FCollisionSystem(FTestEntityRegistry const& registry) noexcept
    : entity_registry_{registry}
    , uniform_grid_{registry} {}
void FCollisionSystem::rebuild_grid() {
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
void FCollisionSystem::collect_overlaps_for_moved_entities(
    std::span<FRegistryEntityHandle const> const collision_dirty_entities) {

    overlap_storage_.clear();

    auto const& entity_data{entity_registry_.get_entity_data()};
    for (auto const dirty_entity : collision_dirty_entities) {
        if (!entity_registry_.is_valid_alive(dirty_entity)) {
            continue;
        }

        auto const entity_index{dirty_entity.index};
        auto const entity_type_index{std::to_underlying(entity_data.entity_types[entity_index])};
        auto const bounds{simulation::collision::make_entity_world_bounds(
            entity_aabbs_,
            entity_type_index,
            entity_data.locations[entity_index],
            simulation::to_quaternion(entity_data.rotations[entity_index]))};

        overlapping_entities_scratch_.clear();
        overlapping_static_geometry_indices_scratch_.clear();
        uniform_grid_.append_overlaps(bounds,
                                      dirty_entity,
                                      overlapping_entities_scratch_,
                                      overlapping_static_geometry_indices_scratch_);

        for (auto const overlapping_entity : overlapping_entities_scratch_) {
            overlap_storage_.add_entity_overlap(dirty_entity, overlapping_entity);
        }

        for (auto const static_geometry_index : overlapping_static_geometry_indices_scratch_) {
            overlap_storage_.add_static_overlap(dirty_entity, static_geometry_index);
        }
    }

    overlap_storage_.finalize();
}
}
