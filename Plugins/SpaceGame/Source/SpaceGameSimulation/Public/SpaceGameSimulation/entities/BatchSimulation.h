#pragma once
#include <CoreMinimal.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
struct FTestEntityRegistry;
struct FRegistryEntityHandle;
struct EntityDeathInfo;
namespace ml {
struct FSpatialQueryManager;
}
namespace ml::batch {
SPACEGAMESIMULATION_API void
    sort_and_deduplicate_removal_indices(TArray<int32>& local_indices_to_remove);

SPACEGAMESIMULATION_API void resolve_damage_events(FTestEntityRegistry const& registry,
                                                   TArray<FRegistryEntityHandle>& entity_handles,
                                                   TArray<int32>& healths,
                                                   TArray<int32>& local_indices_to_remove,
                                                   EntityDeathInfo& entity_death_info);

SPACEGAMESIMULATION_API void refresh_targets(FTestEntityRegistry const& registry,
                                             FSpatialQueryManager const& spatial_query_manager,
                                             TArray<FRegistryEntityHandle>& target_handles,
                                             TArray<int32>& indices_without_targets,
                                             TConstArrayView<ETestTeam> const teams,
                                             ETestEntityType const target_type);
}
