#include "sandbox/simulation/collision_overlap_storage.h"

#include "sandbox/simulation/entity_overlap_operations.h"

namespace ml::simulation::collision {
void CollisionOverlapStorage::reset() noexcept {
    clear();
    sort_indices_scratch_.clear();
}

void CollisionOverlapStorage::clear() noexcept {
    entity_entity_overlaps_.reset();
    entity_static_overlaps_.reset();
}

void CollisionOverlapStorage::add_entity_overlap(FRegistryEntityHandle const first,
                                                 FRegistryEntityHandle const second) {
    if (second < first) {
        entity_entity_overlaps_.add(second, first);
    } else {
        entity_entity_overlaps_.add(first, second);
    }
}

void CollisionOverlapStorage::add_static_overlap(FRegistryEntityHandle const entity,
                                                 std::int32_t const static_geometry_index) {
    entity_static_overlaps_.add(entity, static_geometry_index);
}

void CollisionOverlapStorage::finalize() {
    sort_and_deduplicate(entity_entity_overlaps_, sort_indices_scratch_);
    sort_and_deduplicate(entity_static_overlaps_, sort_indices_scratch_);
}

auto CollisionOverlapStorage::get_view() const noexcept -> DetectedOverlapsView {
    return {entity_entity_overlaps_.get_const_view(), entity_static_overlaps_.get_const_view()};
}

auto CollisionOverlapStorage::entity_entity_overlaps() const noexcept
    -> ioj::FEntityEntityOverlapsConstView {
    return entity_entity_overlaps_.get_const_view();
}

auto CollisionOverlapStorage::entity_static_overlaps() const noexcept
    -> ioj::FEntityStaticOverlapsConstView {
    return entity_static_overlaps_.get_const_view();
}
} // namespace ml::simulation::collision
