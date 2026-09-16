#include "ioj/sim/batch_operations.h"

#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/health.h>
#include <ioj/sim/profiling.h>

#include <algorithm>
#include <cassert>
#include <functional>

namespace ioj::sim::batch {
void sort_and_deduplicate_removal_indices(std::vector<std::int32_t>& local_indices_to_remove) {
    std::ranges::sort(local_indices_to_remove, std::greater{});
    auto const unique_end{std::ranges::unique(local_indices_to_remove).begin()};
    local_indices_to_remove.erase(unique_end, local_indices_to_remove.end());
}

void resolve_damage_events(EntityRegistry const& registry,
                           std::span<RegistryEntityHandle const> entity_handles,
                           std::span<std::int32_t> healths,
                           std::vector<std::int32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info) {
    SANDBOX_PROFILE_SCOPE("batch::resolve_damage_events");

    auto const& direct_view{registry.get_direct_damage_queue_view()};
    auto const n_direct_events{direct_view.num()};
    auto const removal_count{local_indices_to_remove.size()};
    auto const death_count{entity_death_info.num()};
    local_indices_to_remove.resize(removal_count + static_cast<std::size_t>(n_direct_events));
    entity_death_info.add_uninitialised(n_direct_events);

    assert(healths.size() == entity_handles.size());
    auto current_removal_count{static_cast<std::int32_t>(removal_count)};
    auto current_death_count{death_count};
    auto const damage_events{direct_view.get_const_view()};
    for (std::int32_t event_index{}; event_index < n_direct_events; ++event_index) {
        auto const element{static_cast<std::size_t>(event_index)};
        auto const damaged_handle{damage_events.damaged_entities[element]};
        auto const entity{std::ranges::find(entity_handles, damaged_handle)};
        if (entity == entity_handles.end()) {
            continue;
        }

        auto const local_index{static_cast<std::int32_t>(entity - entity_handles.begin())};
        auto const local_element{static_cast<std::size_t>(local_index)};
        healths[local_element] -= damage_events.damage_amounts[element];
        auto const removals{std::span{local_indices_to_remove}.first(
            static_cast<std::size_t>(current_removal_count))};
        if (is_alive(healths[local_element]) ||
            std::ranges::find(removals, local_index) != removals.end()) {
            continue;
        }

        local_indices_to_remove[static_cast<std::size_t>(current_removal_count++)] = local_index;
        auto const instigator{damage_events.instigators[element]};
        auto const reason{instigator.is_null() ? DeathReason::Unknown : DeathReason::Combat};
        entity_death_info.set(
            current_death_count++, reason, entity_handles[local_element], instigator);
    }
    local_indices_to_remove.resize(static_cast<std::size_t>(current_removal_count));
    entity_death_info.set_num(current_death_count);
}

}
