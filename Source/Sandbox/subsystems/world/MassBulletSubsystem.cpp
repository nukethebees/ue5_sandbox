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

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto& cmd_buffer{entity_manager.Defer()};

    RETURN_IF_NULLPTR(visualization_actor);
    TRY_INIT_OPTIONAL(idx, visualization_actor->add_instance(spawn_transform));

    if (free_list.Num()) {
        logger.log_verbose(TEXT("Popping from list."));
        auto entity{free_list.Pop()};
        configure_active_bullet(entity_manager, entity, spawn_transform, bullet_speed, *idx);
        return;
    }
    logger.log_verbose(TEXT("Queuing async creation."));

    RETURN_IF_INVALID(bullet_archetype);
    cmd_buffer.PushCommand<FMassDeferredCreateCommand>(
        [i = *idx, spawn_transform, bullet_speed, this](FMassEntityManager& entity_manager) {
            TRACE_CPUPROFILER_EVENT_SCOPE(
                TEXT("Sandbox::UMassBulletSubsystem::add_bullet::Command"))
            logger.log_verbose(TEXT("Async creation starting."));

            auto entity{entity_manager.CreateEntity(bullet_archetype, shared_values)};
            configure_active_bullet(entity_manager, entity, spawn_transform, bullet_speed, i);
        });
}
void UMassBulletSubsystem::return_bullet(FMassEntityHandle handle) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::return_bullet"))
    constexpr auto logger{NestedLogger<"return_bullet">()};
    logger.log_verbose(TEXT("Returning bullet."));
    free_list.Add(handle);
}

void UMassBulletSubsystem::OnWorldBeginPlay(UWorld& world) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::OnWorldBeginPlay"))
    constexpr auto logger{NestedLogger<"OnWorldBeginPlay">()};
    Super::OnWorldBeginPlay(world);

    if (!initialise_asset_data()) {
        logger.log_warning(TEXT("Failed to load the asset data."));
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

    auto& streamable_manager{UAssetManager::GetStreamableManager()};
    FSoftObjectPath asset_path{TEXT("/Game/DataAssets/Bullet.Bullet")};
    auto* loaded_obj{asset_path.TryLoad()};

    // INIT_PTR_OR_RETURN_VALUE(loaded_obj, asset_manager.GetPrimaryAssetObject(primary_asset_id),
    // false);
    INIT_PTR_OR_RETURN_VALUE(loaded_data, Cast<UBulletDataAsset>(loaded_obj), false);
    bullet_data = loaded_data;

    return true;
}
void UMassBulletSubsystem::configure_active_bullet(FMassEntityManager& entity_manager,
                                                   FMassEntityHandle entity,
                                                   FTransform const& transform,
                                                   float bullet_speed,
                                                   int32 ismc_index) {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::UMassBulletSubsystem::add_bullet::configure_active_bullet"))
    constexpr auto logger{NestedLogger<"configure_active_bullet">()};
    logger.log_verbose(TEXT("Configuring bullet."));

    auto& transform_frag{
        entity_manager.GetFragmentDataChecked<FMassBulletTransformFragment>(entity)};
    transform_frag.transform = transform;

    auto& velocity_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletVelocityFragment>(entity);
    auto const velocity{transform.Rotator().Vector().GetSafeNormal() * bullet_speed};
    velocity_frag.velocity = velocity;

    auto& index_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletInstanceIndexFragment>(entity);
    index_frag.instance_index = ismc_index;

    auto& last_pos_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletLastPositionFragment>(entity);
    last_pos_frag.last_position = transform.GetLocation();

    auto& hit_info_frag = entity_manager.GetFragmentDataChecked<FMassBulletHitInfoFragment>(entity);
    hit_info_frag.hit_location = FVector::ZeroVector;
    hit_info_frag.hit_normal = FVector::ZeroVector;

    auto& state_frag = entity_manager.GetFragmentDataChecked<FMassBulletStateFragment>(entity);
    state_frag.hit_occurred = false;
}
