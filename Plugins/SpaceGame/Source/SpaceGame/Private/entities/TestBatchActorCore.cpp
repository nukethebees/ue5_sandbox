#include "SpaceGame/entities/TestBatchActorCore.h"

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/DirectDamageEvents.h>
#include <SpaceGame/entities/EntityDeathInfo.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/simulation/SpatialQueryManager.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>
#include <Templates/Greater.h>

namespace ml::batch {
void configure_ismc(UInstancedStaticMeshComponent& instances, FIsmcConfig const& config) {
    instances.SetMobility(EComponentMobility::Movable);
    check(instances.SetStaticMesh(config.mesh));
    instances.SetMobility(EComponentMobility::Static);

    if (IsValid(config.material)) {
        instances.SetMaterial(0, config.material);
    }

    instances.SetCanEverAffectNavigation(false);
    instances.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances.SetGenerateOverlapEvents(false);
    instances.SetCastShadow(false);
    instances.SetAffectDistanceFieldLighting(false);
    instances.SetReceivesDecals(false);
    instances.SetRemoveSwap();

    if (config.cull_distances.IsSet()) {
        auto const& cull_distances{config.cull_distances.GetValue()};
        instances.SetCullDistances(cull_distances.min_distance, cull_distances.max_distance);
    }

    if (config.num_custom_data_floats.IsSet()) {
        instances.SetNumCustomDataFloats(config.num_custom_data_floats.GetValue());
    }
}

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

void refresh_targets(FTestEntityRegistry const& registry,
                     FSpatialQueryManager const& spatial_query_manager,
                     TArray<FRegistryEntityHandle>& target_handles,
                     TArray<int32>& indices_without_targets,
                     TConstArrayView<ETestTeam> const teams,
                     ETestEntityType const target_type) {
    TRACE_CPUPROFILER_EVENT_SCOPE(ml::batch::refresh_targets);

    indices_without_targets.Reset();
    registry.refresh_handles(target_handles);

    auto const n{target_handles.Num()};
    for (int32 i{0}; i < n; ++i) {
        if (target_handles[i].is_null()) {
            indices_without_targets.Add(i);
        }
    }

    for (int32 const i : indices_without_targets) {
        target_handles[i] = spatial_query_manager.get_any_non_team_entity(teams[i], target_type);
    }
}
}
