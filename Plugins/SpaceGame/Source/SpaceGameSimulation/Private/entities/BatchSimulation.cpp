#include "SpaceGameSimulation/entities/BatchSimulation.h"

#include <sandbox/simulation/batch_damage.h>
#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

namespace ml::batch {
void sort_and_deduplicate_removal_indices(TArray<int32>& local_indices_to_remove) {
    auto const count{ml::simulation::sort_and_deduplicate_removal_indices(
        {local_indices_to_remove.GetData(),
         static_cast<std::size_t>(local_indices_to_remove.Num())})};
    local_indices_to_remove.SetNum(count, EAllowShrinking::No);
}

void resolve_damage_events(FTestEntityRegistry const& registry,
                           TArray<FRegistryEntityHandle>& entity_handles,
                           TArray<int32>& healths,
                           TArray<int32>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info) {
    TRACE_CPUPROFILER_EVENT_SCOPE(ml::batch::resolve_damage_events);

    auto const& direct_view{registry.get_direct_damage_queue_view()};
    auto const n_direct_events{direct_view.num()};
    auto const removal_count{local_indices_to_remove.Num()};
    auto const death_count{entity_death_info.num()};
    local_indices_to_remove.AddUninitialized(n_direct_events);
    entity_death_info.add_uninitialised(n_direct_events);

    auto const result{ml::simulation::resolve_batch_damage(
        {entity_handles.GetData(), static_cast<std::size_t>(entity_handles.Num())},
        {healths.GetData(), static_cast<std::size_t>(healths.Num())},
        direct_view.get_const_view(),
        {local_indices_to_remove.GetData(),
         static_cast<std::size_t>(local_indices_to_remove.Num())},
        removal_count,
        entity_death_info.get_view(),
        death_count)};
    local_indices_to_remove.SetNum(result.removal_count, EAllowShrinking::No);
    entity_death_info.set_num(result.death_count);
}

}
