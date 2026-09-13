#include "sandbox/simulation/laser_lifecycle.h"

#include "sandbox/core/frame_array.h"
#include "sandbox/core/generated/array_math_kernels.h"

#include <algorithm>
#include <cassert>
#include <functional>

namespace ml::simulation::lasers {
void expire_instances(Entities& entities,
                      float const delta_time,
                      std::pmr::memory_resource& frame_memory) {
    ml::subtract_in_place(std::span<float>{entities.lifetimes_remaining}, delta_time);

    FrameArray<std::int32_t> expired_indices{&frame_memory};
    auto const count{entities.num()};
    expired_indices.reserve(count);
    for (std::int32_t index{count - 1}; index >= 0; --index) {
        if (entities.lifetimes_remaining[index] <= 0.f) {
            expired_indices.add(index);
        }
    }

    remove_instances(entities, expired_indices.view());
}

void update_locations(EntitiesView const entities, float const delta_time) {
    ml::add_scaled_in_place(entities.locations.xs, entities.velocities.xs, delta_time);
    ml::add_scaled_in_place(entities.locations.ys, entities.velocities.ys, delta_time);
    ml::add_scaled_in_place(entities.locations.zs, entities.velocities.zs, delta_time);
}

void remove_instances(Entities& entities, std::span<std::int32_t const> const indices) {
    assert(std::ranges::is_sorted(indices, std::greater{}));
    for (auto const index : indices) {
        entities.remove_at_swap(index, 1);
    }
    entities.validate_array_sizes();
}
} // namespace ml::simulation::lasers
