#include "ioj/sim/batch_operations.h"

#include <ioj/column_math.h>
#include <ioj/frame_array.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/entity_death_info.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/health.h>
#include <ioj/sim/profiling.h>

#include <algorithm>
#include <cassert>
#include <functional>

namespace ioj::sim {
void sort_and_deduplicate_removal_indices(std::vector<std::uint32_t>& local_indices_to_remove) {
    std::ranges::sort(local_indices_to_remove, std::greater{});
    auto const unique_end{std::ranges::unique(local_indices_to_remove).begin()};
    local_indices_to_remove.erase(unique_end, local_indices_to_remove.end());
}

void resolve_damage_events(DirectDamageEvents::ConstView damage_events,
                           EntityLookupTable const& lookup,
                           [[maybe_unused]] std::span<EntityId const> entity_ids,
                           vectors3f::ResolvedConstView const locations,
                           HealthView const healths,
                           std::vector<std::uint32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info,
                           EntityLedger& ledger,
                           FrameMemoryResource* const scratch_resource) {
    SANDBOX_PROFILE_SCOPE("resolve_damage_events");

    auto const n_direct_events{damage_events.num()};
    FrameArray<Health> applied_damages{scratch_resource};
    applied_damages.set_num(n_direct_events);
    std::ranges::fill(applied_damages, Health{});
    auto const removal_count{local_indices_to_remove.size()};
    auto const death_count{entity_death_info.num()};
    local_indices_to_remove.resize(removal_count + static_cast<std::size_t>(n_direct_events));
    entity_death_info.add_uninitialised(n_direct_events);

    assert(healths.num() == static_cast<std::uint32_t>(entity_ids.size()));
    auto current_removal_count{static_cast<std::uint32_t>(removal_count)};
    auto current_death_count{death_count};
    auto const removal_storage{std::span{local_indices_to_remove}};
    auto const handles{lookup.entries()};
    auto const damaged_entities{damage_events.damaged_entities()};
    auto const damage_amounts{damage_events.damage_amounts()};
    auto const instigators{damage_events.instigators()};
    auto const deaths{entity_death_info.get_view()};
    auto const death_reasons{deaths.reasons()};
    auto const victims{deaths.victims()};
    auto const killers{deaths.killers()};
    auto const death_locations{deaths.locations()};
    for (std::uint32_t event_index{}; event_index < n_direct_events; ++event_index) {
        auto const element{static_cast<std::size_t>(event_index)};
        auto const id{damaged_entities[element]};
        auto const local_index{handles[id.index()].index()};
        assert(local_index < entity_ids.size());
        assert(entity_ids[local_index] == id);
        if (is_dead(healths.health(local_index))) {
            continue;
        }
        auto const requested_damage{damage_amounts[element]};
        assert(requested_damage >= 0);
        if (requested_damage == 0) {
            continue;
        }
        auto health{healths.health(local_index)};
        auto const applied_damage{Health{std::min(health.value(), requested_damage)}};
        health.set_value(health.value() - requested_damage);
        healths.set_health(local_index, health);
        applied_damages[event_index] = applied_damage;
        // The populated prefix grows as this loop discovers deaths.
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call)
        auto const removals{removal_storage.first(static_cast<std::size_t>(current_removal_count))};
        if (is_alive(health) || std::ranges::find(removals, local_index) != removals.end()) {
            continue;
        }

        local_indices_to_remove[static_cast<std::size_t>(current_removal_count++)] = local_index;
        auto const instigator{instigators[element]};
        auto const reason{instigator.is_valid() ? DeathReason::Combat : DeathReason::Unknown};
        death_reasons[current_death_count] = reason;
        victims[current_death_count] = id;
        killers[current_death_count] = instigator;
        death_locations[current_death_count] = vector_at(locations, local_index);
        ++current_death_count;
    }
    local_indices_to_remove.resize(static_cast<std::size_t>(current_removal_count));
    entity_death_info.set_num(current_death_count);
    ledger.record_damage(damage_events, {applied_damages.data(), applied_damages.num()});
}

}
