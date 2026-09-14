#include "ioj/sim/capital_ship_spawning.h"
#include "sandbox/core/array_math.h"

#include <cassert>

namespace ioj::sim::capitals {
void initialize_spawned_ships(SpawnInitializationView const ships,
                              CapitalSpawnDataConstView const spawns) {
    auto const count{spawns.locations.num()};
    assert(ships.locations.num() == count);
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        ships.locations.set(index, spawns.locations[index]);
        ships.rotations.set(index, spawns.rotations[index]);
        ships.remaining_spawn_times[element] = spawns.initial_spawn_delays[element];
        ships.spawn_cooldowns[element] = spawns.spawn_cooldowns[element];
        ships.teams[element] = static_cast<std::byte>(spawns.teams[element]);
        ships.healths[element] = spawns.healths[element];
        ships.targets[element] = spawns.target_handles[element];
    }
}

void collect_ships_ready_to_spawn_fighters(std::span<float const> const remaining_times,
                                           std::span<RegistryEntityHandle const> const targets,
                                           ml::FrameArray<std::int32_t>& indices) {
    assert(remaining_times.size() == targets.size());
    auto const count{static_cast<std::int32_t>(remaining_times.size())};
    indices.clear();
    if (count == 0) {
        return;
    }
    indices.set_num(count);
    indices.set_num(
        ml::kernel::collect_indices_less_equal(remaining_times.data(), count, 0.f, indices.data()));

    auto const ready_count{indices.num()};
    for (auto index{ready_count - 1}; index >= 0; --index) {
        if (targets[static_cast<std::size_t>(indices[index])].is_null()) {
            indices.remove_at_swap(index);
        }
    }
}
}
