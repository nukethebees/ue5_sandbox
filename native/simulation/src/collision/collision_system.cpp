#include "ioj/sim/collision/collision_system.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <ioj/sim/profiling.h>
#include <ioj/sim/rotator_math.h>
#include <sandbox/core/diagnostics.h>
#include <thread>

#include <ioj/sim/entity_registry.h>

namespace ioj::sim::collision {
void CollisionSystem::initialise(ioj::sim::collision::EntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
    overlap_storage_.reset();
    reset_frame_events();
    overlapping_entities_scratch_.clear();
    overlapping_static_geometry_indices_scratch_.clear();
}
auto CollisionSystem::update(std::span<RegistryEntityHandle const> const collision_dirty_entities,
                             ioj::sim::SimTick const tick) -> DetectedOverlapsView {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionSystem::update");
    rebuild_grid();
    collect_overlaps_for_moved_entities(collision_dirty_entities);

    overlap_event_storage_.append_batch(
        tick, overlap_storage_.entity_entity_overlaps(), overlap_storage_.entity_static_overlaps());

    return overlap_storage_.get_view();
}
void CollisionSystem::reset_frame_events() {
    overlap_event_storage_.reset();
}
CollisionSystem::CollisionSystem(EntityRegistry const& registry) noexcept
    : entity_registry_{registry}
    , uniform_grid_{registry} {}
void CollisionSystem::rebuild_grid() {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionSystem::rebuild_grid");
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
void CollisionSystem::collect_overlaps_for_moved_entities(
    std::span<RegistryEntityHandle const> const collision_dirty_entities) {
    SANDBOX_PROFILE_SCOPE("Sandbox::CollisionSystem::collect_overlaps_for_moved_entities");

    overlap_storage_.clear();

    auto const& entity_data{entity_registry_.get_entity_data()};
    for (auto const dirty_entity : collision_dirty_entities) {
        if (!entity_registry_.is_valid_alive(dirty_entity)) {
            continue;
        }

        auto const entity_index{dirty_entity.index};
        auto const entity_type_index{std::to_underlying(entity_data.entity_types[entity_index])};
        auto const bounds{ioj::sim::collision::make_entity_world_bounds(
            entity_aabbs_,
            entity_type_index,
            entity_data.locations[entity_index],
            ioj::sim::to_quaternion(entity_data.rotations[entity_index]))};

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
