#include "SpaceGameSimulation/simulation/CollisionSystem.h"

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

    auto const entity_entity_offset{entity_entity_overlap_events_.num()};
    auto const entity_static_offset{entity_static_overlap_events_.num()};
    entity_entity_overlap_events_.append_from(entity_entity_overlaps_.get_const_view());
    entity_static_overlap_events_.append_from(entity_static_overlaps_.get_const_view());
    overlap_event_batches_.push_back({
        .tick = tick,
        .entity_entity_overlaps =
            {
                .offset = entity_entity_offset,
                .count = entity_entity_overlaps_.num(),
            },
        .entity_static_overlaps =
            {
                .offset = entity_static_offset,
                .count = entity_static_overlaps_.num(),
            },
    });

    return {entity_entity_overlaps_.get_const_view(), entity_static_overlaps_.get_const_view()};
}
void FCollisionSystem::reset_frame_events() {
    entity_entity_overlap_events_.reset();
    entity_static_overlap_events_.reset();
    overlap_event_batches_.clear();
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

    sort_and_deduplicate_overlaps();
}
void FCollisionSystem::sort_and_deduplicate_overlaps() {
    auto const entity_pair_count{entity_entity_overlaps_.num()};
    if (entity_pair_count > 1) {
        overlap_sort_indices_scratch_.resize(static_cast<std::size_t>(entity_pair_count));
        entity_entity_overlaps_.sort(
            [](FEntityEntityOverlaps const& overlaps, int32 const lhs, int32 const rhs) {
                auto const lhs_first{overlaps.first_entities[lhs]};
                auto const rhs_first{overlaps.first_entities[rhs]};
                return lhs_first < rhs_first ||
                       (lhs_first == rhs_first &&
                        overlaps.second_entities[lhs] < overlaps.second_entities[rhs]);
            },
            std::span{overlap_sort_indices_scratch_});

        int32 write_index{1};
        for (int32 read_index{1}; read_index < entity_pair_count; ++read_index) {
            if (entity_entity_overlaps_.first_entities[read_index] ==
                    entity_entity_overlaps_.first_entities[write_index - 1] &&
                entity_entity_overlaps_.second_entities[read_index] ==
                    entity_entity_overlaps_.second_entities[write_index - 1]) {
                continue;
            }

            entity_entity_overlaps_.set(write_index,
                                        entity_entity_overlaps_.first_entities[read_index],
                                        entity_entity_overlaps_.second_entities[read_index]);
            ++write_index;
        }
        entity_entity_overlaps_.set_num(write_index);
    }

    auto const entity_static_count{entity_static_overlaps_.num()};
    if (entity_static_count > 1) {
        overlap_sort_indices_scratch_.resize(static_cast<std::size_t>(entity_static_count));
        entity_static_overlaps_.sort(
            [](FEntityStaticOverlaps const& overlaps, int32 const lhs, int32 const rhs) {
                auto const lhs_entity{overlaps.entities[lhs]};
                auto const rhs_entity{overlaps.entities[rhs]};
                return lhs_entity < rhs_entity ||
                       (lhs_entity == rhs_entity && overlaps.static_geometry_indices[lhs] <
                                                        overlaps.static_geometry_indices[rhs]);
            },
            std::span{overlap_sort_indices_scratch_});

        int32 write_index{1};
        for (int32 read_index{1}; read_index < entity_static_count; ++read_index) {
            if (entity_static_overlaps_.entities[read_index] ==
                    entity_static_overlaps_.entities[write_index - 1] &&
                entity_static_overlaps_.static_geometry_indices[read_index] ==
                    entity_static_overlaps_.static_geometry_indices[write_index - 1]) {
                continue;
            }

            entity_static_overlaps_.set(
                write_index,
                entity_static_overlaps_.entities[read_index],
                entity_static_overlaps_.static_geometry_indices[read_index]);
            ++write_index;
        }
        entity_static_overlaps_.set_num(write_index);
    }
}
}
