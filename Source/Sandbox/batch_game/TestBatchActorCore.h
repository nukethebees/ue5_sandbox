#pragma once

#include <Sandbox/batch_game/SpatialQueryHit.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <HAL/Platform.h>

struct FTestEntityRegistry;
struct FRegistryEntityHandle;
struct EntityDeathInfo;
class UPrimitiveComponent;

namespace ml::batch {
void resolve_damage_events(FTestEntityRegistry const& registry,
                           TArray<FRegistryEntityHandle>& entity_handles,
                           TArray<int32>& healths,
                           TArray<int32>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info);

void resolve_ismc_hits(TConstArrayView<FSpatialQueryHit> hits,
                       TArrayView<FRegistryEntityHandle> out_entity_handles,
                       UPrimitiveComponent const& expected_component,
                       TConstArrayView<FRegistryEntityHandle> entity_handles);

void refresh_targets(FTestEntityRegistry const& registry,
                     TArray<FRegistryEntityHandle>& target_handles,
                     TArray<int32>& indices_without_targets,
                     TConstArrayView<ETestTeam> const teams,
                     ETestEntityType const target_type);
}
