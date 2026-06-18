#pragma once

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
}
