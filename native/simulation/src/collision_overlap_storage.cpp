#include "ioj/sim/collision_overlap_storage.h"

#include <span>

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
    auto const entity_overlap_count{entity_entity_overlaps_.num()};
    if (entity_overlap_count > 1) {
        sort_indices_scratch_.resize(static_cast<std::size_t>(entity_overlap_count));
        entity_entity_overlaps_.sort(
            [](EntityEntityOverlaps const& values, std::int32_t const lhs, std::int32_t const rhs) {
                auto const lhs_first{values.first_entities[lhs]};
                auto const rhs_first{values.first_entities[rhs]};
                return lhs_first < rhs_first ||
                       (lhs_first == rhs_first &&
                        values.second_entities[lhs] < values.second_entities[rhs]);
            },
            std::span{sort_indices_scratch_});

        std::int32_t write_index{1};
        for (std::int32_t read_index{1}; read_index < entity_overlap_count; ++read_index) {
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

    auto const static_overlap_count{entity_static_overlaps_.num()};
    if (static_overlap_count > 1) {
        sort_indices_scratch_.resize(static_cast<std::size_t>(static_overlap_count));
        entity_static_overlaps_.sort(
            [](EntityStaticOverlaps const& values, std::int32_t const lhs, std::int32_t const rhs) {
                auto const lhs_entity{values.entities[lhs]};
                auto const rhs_entity{values.entities[rhs]};
                return lhs_entity < rhs_entity ||
                       (lhs_entity == rhs_entity &&
                        values.static_geometry_indices[lhs] < values.static_geometry_indices[rhs]);
            },
            std::span{sort_indices_scratch_});

        std::int32_t write_index{1};
        for (std::int32_t read_index{1}; read_index < static_overlap_count; ++read_index) {
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
