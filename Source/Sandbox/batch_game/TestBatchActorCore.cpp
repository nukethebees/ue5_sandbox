#include "TestBatchActorCore.h"

#include <Sandbox/batch_game/test_entity_registry/DamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestEntityType.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

namespace ml::batch {
void resolve_hit_events(ATestEntityRegistry const& registry,
                        TestEntityOwnerId const owner_id,
                        TArray<FRegistryEntityHandle>& entity_handles,
                        TArray<int32>& healths,
                        TArray<int32>& local_indices_to_remove,
                        EntityDeathInfo& entity_death_info) {
    auto const view{registry.get_damage_queue_view(owner_id)};
    auto const n{view.num()};

    for (int32 i{0}; i < n; ++i) {
        auto const ismc_index_hit{view.hit_items[i]};

        healths[ismc_index_hit] -= view.damage_amounts[i];
        if ((healths[ismc_index_hit] <= 0) && !local_indices_to_remove.Contains(ismc_index_hit)) {
            local_indices_to_remove.Add(ismc_index_hit);
            entity_death_info.add(
                ETestDeathReason::Combat, entity_handles[ismc_index_hit], view.instigators[i]);
        }
    }
}

void refresh_targets(ATestEntityRegistry const& registry,
                     TArray<FRegistryEntityHandle>& target_handles,
                     TArray<int32>& indices_without_targets,
                     TConstArrayView<ETestTeam> const teams,
                     ETestEntityType const target_type) {
    indices_without_targets.Reset();
    registry.refresh_handles(target_handles);

    auto const n{target_handles.Num()};
    for (int32 i{0}; i < n; ++i) {
        if (target_handles[i].is_null()) { indices_without_targets.Add(i); }
    }

    for (int32 const i : indices_without_targets) {
        target_handles[i] = registry.get_any_non_team_entity(teams[i], target_type);
    }
}
}
