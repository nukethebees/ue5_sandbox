#include "Sandbox/subsystems/world/MassBulletSubsystem.h"

#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"
#include "MassEntitySubsystem.h"

#include "Sandbox/actors/MassBulletVisualizationActor.h"
#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/generated/data_asset_registries/BulletAssetRegistry.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/subsystems/world/MassArchetypeSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

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

    FCoreDelegates::OnEndFrame.AddUObject(this, &UMassBulletSubsystem::on_end_frame);
}
void UMassBulletSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::Initialize"))
    Super::Initialize(collection);

    (void)queue.logged_init(n_queue_elements, "MassBulletSubsystem");
}
void UMassBulletSubsystem::Deinitialize() {
    FCoreDelegates::OnEndFrame.RemoveAll(this);
    Super::Deinitialize();
}
bool UMassBulletSubsystem::initialise_asset_data() {
    constexpr auto logger{NestedLogger<"handle_assets_ready">()};
    INIT_PTR_OR_RETURN_VALUE(loaded_data, ml::BulletAssetRegistry::get_bullet(), false);
    bullet_data = loaded_data;
    return true;
}
void UMassBulletSubsystem::configure_active_bullet(FMassEntityManager& entity_manager,
                                                   FMassEntityHandle entity,
                                                   FTransform const& transform,
                                                   float bullet_speed) {
    constexpr auto logger{NestedLogger<"configure_active_bullet">()};
    logger.log_verbose(TEXT("Configuring bullet."));

    auto& transform_frag{
        entity_manager.GetFragmentDataChecked<FMassBulletTransformFragment>(entity)};
    transform_frag.transform = transform;

    auto& velocity_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletVelocityFragment>(entity);
    auto const velocity{transform.Rotator().Vector().GetSafeNormal() * bullet_speed};
    velocity_frag.velocity = velocity;

    auto& last_pos_frag =
        entity_manager.GetFragmentDataChecked<FMassBulletLastPositionFragment>(entity);
    last_pos_frag.last_position = transform.GetLocation();

    auto& hit_info_frag = entity_manager.GetFragmentDataChecked<FMassBulletHitInfoFragment>(entity);
    hit_info_frag.hit_location = FVector::ZeroVector;
    hit_info_frag.hit_normal = FVector::ZeroVector;

    auto& state_frag = entity_manager.GetFragmentDataChecked<FMassBulletStateFragment>(entity);
    state_frag.hit_occurred = false;
}

void UMassBulletSubsystem::on_end_frame() {
    if (auto* world{GetWorld()}) {
        if (!world->IsGameWorld()) {
            return;
        }
        if (world->IsPaused()) {
            return;
        }
    }

    queue.swap_and_visit([this](auto const& result) {
        constexpr auto logger{NestedLogger<"on_end_frame">()};

        if (result.full_count > 0) {
            logger.log_warning(TEXT("Queue was full %zu times."), result.full_count);
        }
        if (result.uninitialised_count > 0) {
            logger.log_error(TEXT("Queue was uninitialised %zu times."),
                             result.uninitialised_count);
        }

        if (result.view.transforms.empty()) {
            return;
        }

        consume_spawn_requests(result.view);
    });
}

void UMassBulletSubsystem::consume_spawn_requests(FBulletSpawnRequestView const& requests) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::consume_spawn_requests"))
    constexpr auto logger{NestedLogger<"consume_spawn_requests">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto& cmd_buffer{entity_manager.Defer()};

    RETURN_IF_NULLPTR(visualization_actor);
    RETURN_IF_INVALID(bullet_archetype);

    auto const n{static_cast<int32>(requests.transforms.size())};
    logger.log_verbose(TEXT("Processing %d spawn requests."), n);

    // Phase 1: Process entities from free list
    auto const free_list_count{FMath::Min(free_list.Num(), n)};

    if (free_list_count) {
        logger.log_verbose(TEXT("Popping %d entities from the free list"), free_list_count);
    }

    for (int32 i{0}; i < free_list_count; ++i) {
        auto const& spawn_transform{requests.transforms[i]};
        auto const bullet_speed{requests.speeds[i]};

        auto entity{free_list.Pop()};
        configure_active_bullet(entity_manager, entity, spawn_transform, bullet_speed);
    }

    // Phase 2: Batch create remaining entities
    auto const remaining_count{n - free_list_count};
    if (remaining_count <= 0) {
        return;
    }

    logger.log_verbose(TEXT("Batch creating %d entities."), remaining_count);

    cmd_buffer.PushCommand<FMassDeferredCreateCommand>(
        [requests, free_list_count, remaining_count, this](FMassEntityManager& entity_manager) {
            TRACE_CPUPROFILER_EVENT_SCOPE(
                TEXT("Sandbox::UMassBulletSubsystem::consume_spawn_requests::BatchCreate"))
            constexpr auto logger{NestedLogger<"consume_spawn_requests">()};

            TArray<FMassEntityHandle> new_entities{};
            entity_manager.BatchCreateEntities(
                bullet_archetype, shared_values, remaining_count, new_entities);

            logger.log_verbose(TEXT("Configuring %d batch created entities."), new_entities.Num());

            auto const n{new_entities.Num()};
            for (int32 i{0}; i < n; ++i) {
                auto const request_idx{free_list_count + i};
                configure_active_bullet(entity_manager,
                                        new_entities[i],
                                        requests.transforms[request_idx],
                                        requests.speeds[request_idx]);
            }
        });
    entity_manager.FlushCommands();
}
