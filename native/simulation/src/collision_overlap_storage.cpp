#include "ioj/sim/collision_overlap_storage.h"

#include "ioj/sim/entity_overlap_operations.h"

namespace ioj::sim::collision {
void CollisionOverlapStorage::reset() noexcept {
    clear();
    sort_indices_scratch_.clear();
}

void CollisionOverlapStorage::clear() noexcept {
    entity_entity_overlaps_.reset();
    entity_static_overlaps_.reset();
}

void CollisionOverlapStorage::add_entity_overlap(RegistryEntityHandle const first,
                                                 RegistryEntityHandle const second) {
    if (second < first) {
        entity_entity_overlaps_.add(second, first);
    } else {
        entity_entity_overlaps_.add(first, second);
    }
}

void CollisionOverlapStorage::add_static_overlap(RegistryEntityHandle const entity,
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
    -> ioj::sim::collision::EntityEntityOverlapsConstView {
    return entity_entity_overlaps_.get_const_view();
}

auto CollisionOverlapStorage::entity_static_overlaps() const noexcept
    -> ioj::sim::collision::EntityStaticOverlapsConstView {
    return entity_static_overlaps_.get_const_view();
}
} // namespace ioj::sim::collision
