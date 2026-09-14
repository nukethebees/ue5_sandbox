#include "ioj/sim/entity_registry_spawn.h"

#include <cassert>
#include <cstddef>

namespace ioj::sim {
auto register_spawned_entity(EntityRegistryBookkeeping& bookkeeping,
                             EntityRegistryStatistics& statistics,
                             EntityHistoryColumnsView const history,
                             std::int32_t const slot_index,
                             EntityUniqueId const unique_id,
                             Team const team,
                             EntityType const type,
                             std::uint8_t const alive) noexcept -> RegistryEntityHandle {
    assert(slot_index >= 0);
    assert(static_cast<std::size_t>(slot_index) < bookkeeping.generations.size());
    assert(bookkeeping.unique_ids.size() == bookkeeping.generations.size());
    assert(unique_id.id >= 0 && unique_id.id < history.num());

    auto const slot_element{static_cast<std::size_t>(slot_index)};
    auto const unique_element{static_cast<std::size_t>(unique_id.id)};
    auto const generation{bookkeeping.generations[slot_element]};
    bookkeeping.unique_ids[slot_element] = unique_id;
    history.registry_indices[unique_element] = slot_index;
    history.registry_generations[unique_element] = generation;
    history.life_state[unique_element] = alive != 0 ? LifeState::Alive : LifeState::Unknown;
    history.entity_types[unique_element] = type;
    history.teams[unique_element] = team;

    statistics.record_spawn(team, type, alive != 0);
    return {slot_index, generation};
}
} // namespace ioj::sim
