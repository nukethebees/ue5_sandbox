#pragma once
#include <CoreMinimal.h>
struct FTestEntityRegistry;
struct FRegistryEntityHandle;
struct EntityDeathInfo;
namespace ml::batch {
SPACEGAMESIMULATION_API void
    sort_and_deduplicate_removal_indices(TArray<int32>& local_indices_to_remove);

SPACEGAMESIMULATION_API void resolve_damage_events(FTestEntityRegistry const& registry,
                                                   TArray<FRegistryEntityHandle>& entity_handles,
                                                   TArray<int32>& healths,
                                                   TArray<int32>& local_indices_to_remove,
                                                   EntityDeathInfo& entity_death_info);

}
