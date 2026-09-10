#include "SpaceGameSimulation/entities/BatchSimulation.h"

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/entities/EntityDeathInfo.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>
#include <Templates/Greater.h>

namespace ml::batch {
void sort_and_deduplicate_removal_indices(TArray<int32>& local_indices_to_remove) {
    local_indices_to_remove.Sort(TGreater<int32>{});

    auto const num_indices{local_indices_to_remove.Num()};
    if (num_indices < 2) {
        return;
    }

    int32 write_index{1};
    for (int32 read_index{1}; read_index < num_indices; ++read_index) {
        auto const local_index{local_indices_to_remove[read_index]};
        if (local_index == local_indices_to_remove[write_index - 1]) {
            continue;
        }

        local_indices_to_remove[write_index] = local_index;
        ++write_index;
    }

    local_indices_to_remove.SetNum(write_index, EAllowShrinking::No);
}

namespace {
void apply_damage(int32 const local_index,
                  int32 const damage_amount,
                  FRegistryEntityHandle const instigator,
                  TArray<FRegistryEntityHandle>& entity_handles,
                  TArray<int32>& healths,
                  TArray<int32>& local_indices_to_remove,
                  EntityDeathInfo& entity_death_info) {
    healths[local_index] -= damage_amount;
    if ((healths[local_index] > 0) || local_indices_to_remove.Contains(local_index)) {
        return;
    }

    local_indices_to_remove.Add(local_index);

    ETestDeathReason const reason{instigator.is_null() ? ETestDeathReason::Unknown
                                                       : ETestDeathReason::Combat};
    entity_death_info.add(reason, entity_handles[local_index], instigator);
}
}

void resolve_damage_events(FTestEntityRegistry const& registry,
                           TArray<FRegistryEntityHandle>& entity_handles,
                           TArray<int32>& healths,
                           TArray<int32>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info) {
    TRACE_CPUPROFILER_EVENT_SCOPE(ml::batch::resolve_damage_events);

    auto const& direct_view{registry.get_direct_damage_queue_view()};
    auto const n_direct_events{direct_view.num()};

    for (int32 i{0}; i < n_direct_events; ++i) {
        auto const local_index{entity_handles.Find(direct_view.damaged_entities[i])};
        if (local_index == INDEX_NONE) {
            continue;
        }

        apply_damage(local_index,
                     direct_view.damage_amounts[i],
                     direct_view.instigators[i],
                     entity_handles,
                     healths,
                     local_indices_to_remove,
                     entity_death_info);
    }
}

}
