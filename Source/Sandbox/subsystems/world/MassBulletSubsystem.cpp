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

void UMassBulletSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::Initialize"))
    Super::Initialize(collection);
    constexpr auto logger{NestedLogger<"Initialize">()};

    collection.InitializeDependency(UMassArchetypeSubsystem::StaticClass());
    collection.InitializeDependency(UMassEntitySubsystem::StaticClass());

    (void)spawn_queue.logged_init(n_queue_elements, "MassBulletSubsystem::SpawnQueue");
    (void)destroy_queue.logged_init(n_queue_elements, "MassBulletSubsystem::DestroyQueue");

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(archetype_subsystem, world->GetSubsystem<UMassArchetypeSubsystem>());
    archetype_subsystem->on_mass_archetype_subsystem_ready.AddUObject(
        this, &UMassBulletSubsystem::on_begin_play);

    logger.log_display(TEXT("Initialised."));
}
void UMassBulletSubsystem::OnWorldBeginPlay(UWorld& world) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::OnWorldBeginPlay"))
    constexpr auto logger{NestedLogger<"OnWorldBeginPlay">()};
    Super::OnWorldBeginPlay(world);
}
void UMassBulletSubsystem::Deinitialize() {
    FCoreDelegates::OnEndFrame.RemoveAll(this);
    Super::Deinitialize();
}

void UMassBulletSubsystem::on_begin_play() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::on_begin_play"))
    constexpr auto logger{NestedLogger<"on_begin_play">()};

    RETURN_IF_FALSE(initialise_asset_data());

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    TRY_INIT_PTR(archetype_subsystem, world->GetSubsystem<UMassArchetypeSubsystem>());
    RETURN_IF_NULLPTR(bullet_data);

    TRY_INIT_OPTIONAL(entity_def,
                      archetype_subsystem->get_definition(bullet_data->GetPrimaryAssetId()));
    bullet_definition = *entity_def;

    FCoreDelegates::OnEndFrame.AddUObject(this, &UMassBulletSubsystem::on_end_frame);

    logger.log_display(TEXT("Ready."));
}
bool UMassBulletSubsystem::initialise_asset_data() {
    constexpr auto logger{NestedLogger<"handle_assets_ready">()};

    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), false);
    INIT_PTR_OR_RETURN_VALUE(gi, world->GetGameInstance(), false);
    INIT_PTR_OR_RETURN_VALUE(ss, gi->GetSubsystem<UBulletAssetRegistry>(), false);
    INIT_PTR_OR_RETURN_VALUE(loaded_data, ss->get_bullet(), false);
    bullet_data = loaded_data;
    return true;
}
void UMassBulletSubsystem::configure_active_bullet(FMassEntityManager& entity_manager,
                                                   FMassEntityHandle entity,
                                                   FTransform const& transform,
                                                   float bullet_speed) {
    constexpr auto logger{NestedLogger<"configure_active_bullet">()};

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
    hit_info_frag.hit_actor = nullptr;

    auto& state_frag = entity_manager.GetFragmentDataChecked<FMassBulletStateFragment>(entity);
    state_frag.hit_occurred = false;
}
void UMassBulletSubsystem::on_end_frame() {
    TRY_INIT_PTR(world, GetWorld());

    if (!world->IsGameWorld()) {
        return;
    }
    if (world->IsPaused()) {
        return;
    }

    auto spawns{spawn_queue.swap_and_consume()};
    spawns.log_results(TEXT("UMassBulletSubsystem::create"));

    auto destroys{destroy_queue.swap_and_consume()};
    destroys.log_results(TEXT("UMassBulletSubsystem::destroys"));

    consume_lifecycle_requests(spawns.view, destroys.view);
}
void UMassBulletSubsystem::consume_lifecycle_requests(
    FBulletSpawnRequestView const& spawn_requests, std::span<FMassEntityHandle> destroy_requests) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::consume_lifecycle_requests"))
    constexpr auto logger{NestedLogger<"consume_lifecycle_requests">()};

    auto const n_destroy_requests{static_cast<int32>(destroy_requests.size())};
    auto const n_spawn_requests{static_cast<int32>(spawn_requests.transforms.size())};
    auto const n_requests{n_spawn_requests + n_destroy_requests};

    if (n_requests == 0) {
        return;
    }

    RETURN_IF_INVALID(bullet_definition);
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto& cmd_buffer{entity_manager.Defer()};

    // Dequeue destruction requests into free_list
    free_list.Append(destroy_requests.data(), n_destroy_requests);

    // Reuse entities from free_list for spawn requests
    auto const n_reused{FMath::Min(free_list.Num(), n_spawn_requests)};
    for (int32 i{0}; i < n_reused; ++i) {
        configure_active_bullet(entity_manager,
                                free_list.Pop(),
                                spawn_requests.transforms[i],
                                spawn_requests.speeds[i]);
    }

    // Destroy remaining entities in free_list
    auto const n_destroyed{free_list.Num()};
    cmd_buffer.PushCommand<FMassDeferredDestroyCommand>([&](FMassEntityManager& entity_manager) {
        entity_manager.BatchDestroyEntities(free_list);
    });

    // Batch create new entities for unfulfilled spawn requests
    auto const remaining_spawn_requests{n_spawn_requests - n_reused};
    if (remaining_spawn_requests > 0) {
        logger.log_verbose(TEXT("Batch creating %d new entities."), remaining_spawn_requests);
        cmd_buffer.PushCommand<FMassDeferredCreateCommand>(
            [spawn_requests, n_reused, remaining_spawn_requests, this](
                FMassEntityManager& entity_manager) {
                TRACE_CPUPROFILER_EVENT_SCOPE(
                    TEXT("Sandbox::UMassBulletSubsystem::consume_lifecycle_requests::BatchCreate"))
                constexpr auto logger{NestedLogger<"consume_lifecycle_requests">()};

                new_entities.Reset(0);
                entity_manager.BatchCreateEntities(bullet_definition.archetype,
                                                   bullet_definition.shared_fragments,
                                                   remaining_spawn_requests,
                                                   new_entities);

                auto const n{new_entities.Num()};
                for (int32 i{0}; i < n; ++i) {
                    auto const request_idx{n_reused + i};
                    configure_active_bullet(entity_manager,
                                            new_entities[i],
                                            spawn_requests.transforms[request_idx],
                                            spawn_requests.speeds[request_idx]);
                }
            });
    }

    logger.log_verbose(TEXT("Entities: %d created (%d reused, %d created), %d destroyed."),
                       n_spawn_requests,
                       n_reused,
                       remaining_spawn_requests,
                       n_destroyed);

    entity_manager.FlushCommands();
    free_list.Reset(0);
}
