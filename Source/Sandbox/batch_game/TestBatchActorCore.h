#pragma once

#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

class ATestEntityRegistry;
struct TestEntityOwnerId;
struct FRegistryEntityHandle;
struct EntityDeathInfo;

namespace ml::batch {
void resolve_hit_events(ATestEntityRegistry const& registry,
                        TestEntityOwnerId const owner_id,
                        TArray<FRegistryEntityHandle>& entity_handles,
                        TArray<int32>& healths,
                        TArray<int32>& local_indices_to_remove,
                        EntityDeathInfo& entity_death_info);

void refresh_targets(ATestEntityRegistry const& registry,
                     TArray<FRegistryEntityHandle>& target_handles,
                     TArray<int32>& indices_without_targets,
                     TConstArrayView<ETestTeam> const teams,
                     ETestEntityType const target_type);
}
