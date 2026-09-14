#include "ioj/sim/batch_operations.h"

#include <ioj/sim/batch_damage.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_registry.h>
#include <ioj/sim/profiling.h>

namespace ioj::sim::batch {
void sort_and_deduplicate_removal_indices(std::vector<std::int32_t>& local_indices_to_remove) {
    auto const count{ioj::sim::sort_and_deduplicate_removal_indices(
        {local_indices_to_remove.data(),
         static_cast<std::size_t>(local_indices_to_remove.size())})};
    local_indices_to_remove.resize(count);
}

void resolve_damage_events(EntityRegistry const& registry,
                           std::span<RegistryEntityHandle const> entity_handles,
                           std::span<std::int32_t> healths,
                           std::vector<std::int32_t>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info) {
    SANDBOX_PROFILE_SCOPE("ioj::sim::batch::resolve_damage_events");

    auto const& direct_view{registry.get_direct_damage_queue_view()};
    auto const n_direct_events{direct_view.num()};
    auto const removal_count{local_indices_to_remove.size()};
    auto const death_count{entity_death_info.num()};
    local_indices_to_remove.resize(removal_count + static_cast<std::size_t>(n_direct_events));
    entity_death_info.add_uninitialised(n_direct_events);

    auto const result{ioj::sim::resolve_batch_damage(
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
