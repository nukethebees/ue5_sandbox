#include "ioj/sim/batch_operations.h"

#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_ledger.h>

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

void resolve_damage_events(DirectDamageEventsConstView damage_events,
                           AgentIndexes const& indexes,
                           [[maybe_unused]] std::span<EntityUniqueId const> entity_ids,
                           std::span<Health> healths,
                           std::vector<std::int32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info,
                           EntityLedger& ledger) {
    SANDBOX_PROFILE_SCOPE("batch::resolve_damage_events");

    auto const n_direct_events{damage_events.num()};
    auto const removal_count{local_indices_to_remove.size()};
    auto const death_count{entity_death_info.num()};
    local_indices_to_remove.resize(removal_count + static_cast<std::size_t>(n_direct_events));
    entity_death_info.add_uninitialised(n_direct_events);

    assert(healths.size() == entity_ids.size());
    auto current_removal_count{static_cast<std::int32_t>(removal_count)};
    auto current_death_count{death_count};
    for (std::int32_t event_index{}; event_index < n_direct_events; ++event_index) {
        auto const element{static_cast<std::size_t>(event_index)};
        auto const id{damage_events.damaged_entities[element]};
        auto const local_index{indexes.find(id)};
        assert(local_index >= 0 && static_cast<std::size_t>(local_index) < entity_ids.size());
        assert(entity_ids[local_index] == id);
        auto const local_element{static_cast<std::size_t>(local_index)};
        if (is_dead(healths[local_element])) {
            continue;
        }
        auto const requested_damage{damage_events.damage_amounts[element]};
        assert(requested_damage >= 0);
        if (requested_damage == 0) {
            continue;
        }
        auto const applied_damage{std::min(healths[local_element], requested_damage)};
        healths[local_element] -= requested_damage;
        ledger.record_damage(id, damage_events.instigators[element], applied_damage);
        auto const removals{std::span{local_indices_to_remove}.first(
            static_cast<std::size_t>(current_removal_count))};
        if (is_alive(healths[local_element]) ||
            std::ranges::find(removals, local_index) != removals.end()) {
            continue;
        }

        local_indices_to_remove[static_cast<std::size_t>(current_removal_count++)] = local_index;
        auto const instigator{damage_events.instigators[element]};
        auto const reason{instigator.is_valid() ? DeathReason::Combat : DeathReason::Unknown};
        entity_death_info.set(current_death_count++, reason, id, instigator);
    }
    local_indices_to_remove.resize(static_cast<std::size_t>(current_removal_count));
    entity_death_info.set_num(current_death_count);
}

}
