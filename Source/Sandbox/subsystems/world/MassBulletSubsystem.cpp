#include "Sandbox/subsystems/world/MassBulletSubsystem.h"

#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassEntitySubsystem.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/subsystems/world/MassArchetypeSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

void UMassBulletSubsystem::add_bullet(FTransform const& spawn_transform, float bullet_speed) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::add_bullet"))

    constexpr auto logger{NestedLogger<"add_bullet">()};

    RETURN_IF_NULLPTR(bullet_data);
    RETURN_IF_INVALID(bullet_archetype);
    RETURN_IF_NULLPTR(visualization_actor);

    TRY_INIT_PTR(world, GetWorld());

    TRY_INIT_OPTIONAL(idx, visualization_actor->add_instance(spawn_transform));

    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto& cmd_buffer{entity_manager.Defer()};

    cmd_buffer.PushCommand<FMassDeferredCreateCommand>(
        [i = *idx, spawn_transform, bullet_speed, this](FMassEntityManager& entity_manager) {
            logger.log_verbose(TEXT("Async creation"));

            auto entity = entity_manager.CreateEntity(bullet_archetype, shared_values);
            entity_manager.AddTagToEntity(entity, FMassBulletActiveTag::StaticStruct());

            auto& transform_frag{
                entity_manager.GetFragmentDataChecked<FMassBulletTransformFragment>(entity)};
            transform_frag.transform = spawn_transform;

            auto& velocity_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletVelocityFragment>(entity);
            auto const velocity{spawn_transform.Rotator().Vector().GetSafeNormal() * bullet_speed};
            velocity_frag.velocity = velocity;

            auto& index_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletInstanceIndexFragment>(entity);
            index_frag.instance_index = i;

            auto& last_pos_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletLastPositionFragment>(entity);
            last_pos_frag.last_position = spawn_transform.GetLocation();

            auto& hit_info_frag =
                entity_manager.GetFragmentDataChecked<FMassBulletHitInfoFragment>(entity);
            hit_info_frag.hit_location = FVector::ZeroVector;
            hit_info_frag.hit_normal = FVector::ZeroVector;
        });
}
void UMassBulletSubsystem::return_bullet(FMassEntityHandle handle) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::return_bullet"))
}

void UMassBulletSubsystem::OnWorldBeginPlay(UWorld& world) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::OnWorldBeginPlay"))
    Super::OnWorldBeginPlay(world);

    if (!initialise_asset_data()) {
        return;
    }

    TRY_INIT_PTR(mass_subsystem, world.GetSubsystem<UMassEntitySubsystem>());
    TRY_INIT_PTR(archetype_subsystem, world.GetSubsystem<UMassArchetypeSubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};

    bullet_archetype = archetype_subsystem->get_bullet_archetype();
    RETURN_IF_INVALID(bullet_archetype);

    for (auto it{TActorIterator<AMassBulletVisualizationActor>(&world)}; it; ++it) {
        visualization_actor = *it;
        break;
    }
    RETURN_IF_NULLPTR(visualization_actor);
    RETURN_IF_NULLPTR(bullet_data);

    auto impact_effect_handle{
        entity_manager.GetOrCreateConstSharedFragment<FMassBulletImpactEffectFragment>(
            bullet_data->impact_effect)};

    auto viz_actor_handle{
        entity_manager.GetOrCreateConstSharedFragment<FMassBulletVisualizationActorFragment>(
            visualization_actor)};

    shared_values.Add(impact_effect_handle);
    shared_values.Add(viz_actor_handle);
    shared_values.Sort();
}
void UMassBulletSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::Initialize"))
    Super::Initialize(collection);
}
bool UMassBulletSubsystem::initialise_asset_data() {
    constexpr auto logger{NestedLogger<"handle_assets_ready">()};
    static FPrimaryAssetId const primary_asset_id{TEXT("Bullet"), TEXT("Standard")};
    auto& asset_manager{UAssetManager::Get()};

    INIT_PTR_OR_RETURN_VALUE(
        loaded_obj, asset_manager.GetPrimaryAssetObject(primary_asset_id), false);
    INIT_PTR_OR_RETURN_VALUE(loaded_data, Cast<UBulletDataAsset>(loaded_obj), false);
    bullet_data = loaded_data;

    return true;
}
