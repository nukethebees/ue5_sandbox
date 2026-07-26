#include "TestBatchActorCore.h"

#include <Sandbox/batch_game/test_entity_registry/CollisionDamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/DirectDamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/EntityDeathInfo.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestEntityType.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

namespace ml::batch {
namespace {
void apply_damage(int32 const local_index,
                  int32 const damage_amount,
                  FRegistryEntityHandle const instigator,
                  TArray<FRegistryEntityHandle>& entity_handles,
                  TArray<int32>& healths,
                  TArray<int32>& local_indices_to_remove,
                  EntityDeathInfo& entity_death_info) {
    healths[local_index] -= damage_amount;
    if ((healths[local_index] > 0) || local_indices_to_remove.Contains(local_index)) { return; }

    local_indices_to_remove.Add(local_index);

    ETestDeathReason const reason{instigator.is_null() ? ETestDeathReason::Unknown
                                                       : ETestDeathReason::Combat};
    entity_death_info.add(reason, entity_handles[local_index], instigator);
}
}

void resolve_collision_damage_events(ATestEntityRegistry const& registry,
                                     TestEntityOwnerId const owner_id,
                                     TArray<FRegistryEntityHandle>& entity_handles,
                                     TArray<int32>& healths,
                                     TArray<int32>& local_indices_to_remove,
                                     EntityDeathInfo& entity_death_info) {
    auto const view{registry.get_collision_damage_queue_view(owner_id)};
    auto const n{view.num()};

    for (int32 i{0}; i < n; ++i) {
        auto const ismc_index_hit{view.hit_items[i]};
        apply_damage(ismc_index_hit,
                     view.damage_amounts[i],
                     view.instigators[i],
                     entity_handles,
                     healths,
                     local_indices_to_remove,
                     entity_death_info);
    }
}

void resolve_direct_damage_events(ATestEntityRegistry const& registry,
                                  TArray<FRegistryEntityHandle>& entity_handles,
                                  TArray<int32>& healths,
                                  TArray<int32>& local_indices_to_remove,
                                  EntityDeathInfo& entity_death_info) {
    auto const& view{registry.get_direct_damage_queue_view()};
    auto const n{view.num()};

    for (int32 i{0}; i < n; ++i) {
        auto const local_index{entity_handles.Find(view.damaged_entities[i])};
        if (local_index == INDEX_NONE) { continue; }

        apply_damage(local_index,
                     view.damage_amounts[i],
                     view.instigators[i],
                     entity_handles,
                     healths,
                     local_indices_to_remove,
                     entity_death_info);
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
