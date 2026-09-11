#include "SpaceGameSimulation/simulation/CollisionSystem.h"

#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/EntityWorldBounds.h>

namespace ml::ioj {
void FCollisionSystem::initialise(FEntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
    overlap_pairs_.Reset();
    overlap_query_scratch_.Reset();
}
void FCollisionSystem::update(
    TConstArrayView<FRegistryEntityHandle> const collision_dirty_entities) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::update);
    rebuild_grid();
    find_overlap_pairs(collision_dirty_entities);
}
FCollisionSystem::FCollisionSystem(FTestEntityRegistry const& registry) noexcept
    : entity_registry_{registry}
    , uniform_grid_{registry} {}
void FCollisionSystem::rebuild_grid() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::rebuild_grid);
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
void FCollisionSystem::find_overlap_pairs(
    TConstArrayView<FRegistryEntityHandle> const collision_dirty_entities) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::find_overlap_pairs);

    overlap_pairs_.Reset();

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

        overlap_query_scratch_.Reset();
        uniform_grid_.append_overlapping_entities(bounds, dirty_entity, overlap_query_scratch_);

        for (auto const overlapping_entity : overlap_query_scratch_) {
            if (overlapping_entity < dirty_entity) {
                overlap_pairs_.Add({overlapping_entity, dirty_entity});
            } else {
                overlap_pairs_.Add({dirty_entity, overlapping_entity});
            }
        }
    }

    overlap_pairs_.Sort();
    auto const pair_count{overlap_pairs_.Num()};
    if (pair_count < 2) {
        return;
    }

    int32 write_index{1};
    for (int32 read_index{1}; read_index < pair_count; ++read_index) {
        if (overlap_pairs_[read_index] == overlap_pairs_[write_index - 1]) {
            continue;
        }

        overlap_pairs_[write_index++] = overlap_pairs_[read_index];
    }
    overlap_pairs_.SetNum(write_index, EAllowShrinking::No);
}
}
