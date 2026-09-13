#include "sandbox/simulation/fighter_damage_response.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void retarget_from_damage(std::span<FRegistryEntityHandle const> const fighter_handles,
                          std::span<std::byte const> const fighter_teams,
                          std::span<FRegistryEntityHandle> const target_handles,
                          DirectDamageEventsConstView const damage_events,
                          EntityRegistryQueryView const registry) noexcept {
    assert(fighter_teams.size() == fighter_handles.size());
    assert(target_handles.size() == fighter_handles.size());

    auto const count{damage_events.num()};
    for (std::int32_t event_index{}; event_index < count; ++event_index) {
        auto const event_element{static_cast<std::size_t>(event_index)};
        auto const damaged_handle{damage_events.damaged_entities[event_element]};
        auto const fighter{std::ranges::find(fighter_handles, damaged_handle)};
        if (fighter == fighter_handles.end()) {
            continue;
        }

        auto const instigator{damage_events.instigators[event_element]};
        if (analyse_handle(registry, instigator) != RegistryHandleState::Active) {
            continue;
        }

        auto const fighter_index{static_cast<std::size_t>(fighter - fighter_handles.begin())};
        auto const instigator_index{static_cast<std::size_t>(instigator.index)};
        if (registry.teams[instigator_index] != fighter_teams[fighter_index]) {
            target_handles[fighter_index] = instigator;
        }
    }
}
} // namespace ml::simulation::fighters
