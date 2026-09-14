#include "ioj/sim/entity_overlap_operations.h"

#include <span>

namespace ioj::sim::collision {
void sort_and_deduplicate(ioj::sim::collision::EntityEntityOverlaps& overlaps,
                          std::vector<std::int32_t>& sort_indices_scratch) {
    auto const overlap_count{overlaps.num()};
    if (overlap_count <= 1) {
        return;
    }

    sort_indices_scratch.resize(static_cast<std::size_t>(overlap_count));
    overlaps.sort(
        [](ioj::sim::collision::EntityEntityOverlaps const& values,
           std::int32_t const lhs,
           std::int32_t const rhs) {
            auto const lhs_first{values.first_entities[lhs]};
            auto const rhs_first{values.first_entities[rhs]};
            return lhs_first < rhs_first ||
                   (lhs_first == rhs_first &&
                    values.second_entities[lhs] < values.second_entities[rhs]);
        },
        std::span{sort_indices_scratch});

    std::int32_t write_index{1};
    for (std::int32_t read_index{1}; read_index < overlap_count; ++read_index) {
        if (overlaps.first_entities[read_index] == overlaps.first_entities[write_index - 1] &&
            overlaps.second_entities[read_index] == overlaps.second_entities[write_index - 1]) {
            continue;
        }

        overlaps.set(
            write_index, overlaps.first_entities[read_index], overlaps.second_entities[read_index]);
        ++write_index;
    }
    overlaps.set_num(write_index);
}

void sort_and_deduplicate(ioj::sim::collision::EntityStaticOverlaps& overlaps,
                          std::vector<std::int32_t>& sort_indices_scratch) {
    auto const overlap_count{overlaps.num()};
    if (overlap_count <= 1) {
        return;
    }

    sort_indices_scratch.resize(static_cast<std::size_t>(overlap_count));
    overlaps.sort(
        [](ioj::sim::collision::EntityStaticOverlaps const& values,
           std::int32_t const lhs,
           std::int32_t const rhs) {
            auto const lhs_entity{values.entities[lhs]};
            auto const rhs_entity{values.entities[rhs]};
            return lhs_entity < rhs_entity ||
                   (lhs_entity == rhs_entity &&
                    values.static_geometry_indices[lhs] < values.static_geometry_indices[rhs]);
        },
        std::span{sort_indices_scratch});

    std::int32_t write_index{1};
    for (std::int32_t read_index{1}; read_index < overlap_count; ++read_index) {
        if (overlaps.entities[read_index] == overlaps.entities[write_index - 1] &&
            overlaps.static_geometry_indices[read_index] ==
                overlaps.static_geometry_indices[write_index - 1]) {
            continue;
        }

        overlaps.set(write_index,
                     overlaps.entities[read_index],
                     overlaps.static_geometry_indices[read_index]);
        ++write_index;
    }
    overlaps.set_num(write_index);
}
} // namespace ioj::sim::collision
