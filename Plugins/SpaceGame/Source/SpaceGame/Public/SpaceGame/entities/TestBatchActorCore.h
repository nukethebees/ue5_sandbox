#pragma once

#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestTeam.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <HAL/Platform.h>
#include <Misc/Optional.h>

struct FTestEntityRegistry;
struct FRegistryEntityHandle;
struct EntityDeathInfo;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::batch {
struct FCullDistances {
    float min_distance{0.0f};
    float max_distance{0.0f};
};

struct FIsmcConfig {
    UStaticMesh* mesh{nullptr};
    UMaterialInterface* material{nullptr};
    TOptional<FCullDistances> cull_distances{NullOpt};
    TOptional<int32> num_custom_data_floats{NullOpt};
};

void configure_ismc(UInstancedStaticMeshComponent& instances, FIsmcConfig const& config);

void sort_and_deduplicate_removal_indices(TArray<int32>& local_indices_to_remove);

void resolve_damage_events(FTestEntityRegistry const& registry,
                           TArray<FRegistryEntityHandle>& entity_handles,
                           TArray<int32>& healths,
                           TArray<int32>& local_indices_to_remove,
                           EntityDeathInfo& entity_death_info);

void refresh_targets(FTestEntityRegistry const& registry,
                     FSpatialQueryManager const& spatial_query_manager,
                     TArray<FRegistryEntityHandle>& target_handles,
                     TArray<int32>& indices_without_targets,
                     TConstArrayView<ETestTeam> const teams,
                     ETestEntityType const target_type);
}
