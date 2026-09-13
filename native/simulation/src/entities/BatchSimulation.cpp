#include "sandbox/simulation/entities/BatchSimulation.h"

#include <sandbox/simulation/batch_damage.h>
#include <sandbox/simulation/entities/DirectDamageEvents.h>
#include <sandbox/simulation/entities/EntityDeathInfo.h>
#include <sandbox/simulation/entities/TestEntityRegistry.h>

namespace ml::batch {
void sort_and_deduplicate_removal_indices(std::vector<std::int32_t>& local_indices_to_remove) {
    auto const count{ml::simulation::sort_and_deduplicate_removal_indices(
        {local_indices_to_remove.data(),
         static_cast<std::size_t>(local_indices_to_remove.size())})};
    local_indices_to_remove.resize(count);
}

void resolve_damage_events(FTestEntityRegistry const& registry,
                           std::span<FRegistryEntityHandle const> entity_handles,
                           std::span<std::int32_t> healths,
                           std::vector<std::int32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info) {

    auto const& direct_view{registry.get_direct_damage_queue_view()};
    auto const n_direct_events{direct_view.num()};
    auto const removal_count{local_indices_to_remove.size()};
    auto const death_count{entity_death_info.num()};
    local_indices_to_remove.resize(removal_count + static_cast<std::size_t>(n_direct_events));
    entity_death_info.add_uninitialised(n_direct_events);

    auto const result{ml::simulation::resolve_batch_damage(
        {entity_handles.data(), static_cast<std::size_t>(entity_handles.size())},
        {healths.data(), static_cast<std::size_t>(healths.size())},
        direct_view.get_const_view(),
        {local_indices_to_remove.data(), static_cast<std::size_t>(local_indices_to_remove.size())},
        static_cast<std::int32_t>(removal_count),
        entity_death_info.get_view(),
        death_count)};
    local_indices_to_remove.resize(result.removal_count);
    entity_death_info.set_num(result.death_count);
}

}
