#include "ioj/sim/entity_registry_update.h"

#include <cassert>
#include <cstddef>
#include <utility>

namespace ioj::sim {
namespace {
template <typename Enum>
auto enum_at(std::span<std::byte const> const values, std::size_t const index) noexcept -> Enum {
    return static_cast<Enum>(std::to_integer<std::uint8_t>(values[index]));
}

[[maybe_unused]] auto arrays_have_size(EntityRegistryUpdateView const view,
                                       std::size_t const size) noexcept -> bool {
    return view.locations.num() == static_cast<std::int32_t>(size) &&
           view.velocities.num() == static_cast<std::int32_t>(size) &&
           view.rotations.num() == static_cast<std::int32_t>(size) && view.healths.size() == size &&
           view.teams.size() == size && view.entity_types.size() == size &&
           view.alive.size() == size;
}

[[maybe_unused]] auto arrays_have_size(EntityRegistryUpdateConstView const view,
                                       std::size_t const size) noexcept -> bool {
    return view.locations.num() == static_cast<std::int32_t>(size) &&
           view.velocities.num() == static_cast<std::int32_t>(size) &&
           view.rotations.num() == static_cast<std::int32_t>(size) && view.healths.size() == size &&
           view.teams.size() == size && view.alive.size() == size;
}
}

auto apply_entity_updates(EntityRegistryBookkeeping& bookkeeping,
                          EntityRegistryStatistics& statistics,
                          EntityHistoryColumnsView const history,
                          EntityRegistryUpdateView const entities,
                          EntityRegistryUpdateConstView const updates) noexcept -> std::int32_t {
    auto const count{updates.num()};
    [[maybe_unused]] auto const size{static_cast<std::size_t>(count)};
    assert(count >= 0);
    assert(arrays_have_size(updates, size));
    assert(arrays_have_size(entities, bookkeeping.generations.size()));
    assert(bookkeeping.unique_ids.size() == bookkeeping.generations.size());
    assert(bookkeeping.queued_update_handles.size() == size);

    for (std::int32_t update_index{}; update_index < count; ++update_index) {
        auto const update_element{static_cast<std::size_t>(update_index)};
        auto const handle{bookkeeping.queued_update_handles[update_element]};
        if (!bookkeeping.is_valid_handle(handle)) {
            return update_index;
        }

        auto const slot_index{handle.index};
        auto const slot_element{static_cast<std::size_t>(slot_index)};
        auto const position_changed{
            entities.locations.xs[slot_element] != updates.locations.xs[update_element] ||
            entities.locations.ys[slot_element] != updates.locations.ys[update_element] ||
            entities.locations.zs[slot_element] != updates.locations.zs[update_element]};
        auto const rotation_changed{
            entities.rotations.pitches[slot_element] != updates.rotations.pitches[update_element] ||
            entities.rotations.yaws[slot_element] != updates.rotations.yaws[update_element] ||
            entities.rotations.rolls[slot_element] != updates.rotations.rolls[update_element]};
        if (position_changed || rotation_changed) {
            bookkeeping.record_moved(handle);
        }

        auto const old_alive{entities.alive[slot_element] != 0};
        auto const new_alive{updates.alive[update_element] != 0};
        auto const old_team{enum_at<Team>(entities.teams, slot_element)};
        auto const new_team{enum_at<Team>(updates.teams, update_element)};
        auto const entity_type{enum_at<EntityType>(entities.entity_types, slot_element)};
        statistics.apply_alive_transition(old_team, new_team, entity_type, old_alive, new_alive);

        entities.teams[slot_element] = static_cast<std::byte>(std::to_underlying(new_team));
        entities.alive[slot_element] = updates.alive[update_element];
        auto const unique_id{bookkeeping.unique_ids[slot_element]};
        auto const history_element{static_cast<std::size_t>(unique_id.id)};
        if (new_alive) {
            history.life_state[history_element] = LifeState::Alive;
        } else if (old_alive) {
            history.life_state[history_element] = LifeState::Unknown;
        }
        if (old_team != new_team) {
            history.teams[history_element] = new_team;
        }

        entities.locations.set(slot_index, updates.locations[update_index]);
        entities.velocities.set(slot_index, updates.velocities[update_index]);
        entities.rotations.set(slot_index, updates.rotations[update_index]);
        entities.healths[slot_element] = updates.healths[update_element];
    }
    return -1;
}
} // namespace ioj::sim
