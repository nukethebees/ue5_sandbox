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

    collection.InitializeDependency(UMassArchetypeSubsystem::StaticClass());
    collection.InitializeDependency(UMassEntitySubsystem::StaticClass());

    (void)spawn_queue.logged_init(n_queue_elements, "MassBulletSubsystem::SpawnQueue");
    (void)destroy_queue.logged_init(n_queue_elements, "MassBulletSubsystem::DestroyQueue");
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

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto& cmd_buffer{entity_manager.Defer()};

    RETURN_IF_NULLPTR(visualization_actor);
    RETURN_IF_INVALID(bullet_archetype);

    // Dequeue destruction requests into free_list
    if (!destroy_requests.empty()) {
        logger.log_verbose(TEXT("Adding %zu entities to free_list from destroy queue."),
                           destroy_requests.size());
        free_list.Append(destroy_requests.data(), static_cast<int32>(destroy_requests.size()));
    }

    auto const n_spawn_requests{static_cast<int32>(spawn_requests.transforms.size())};
    logger.log_verbose(TEXT("Processing %d spawn requests."), n_spawn_requests);

    // Reuse entities from free_list for spawn requests
    auto const entity_reuse_count{FMath::Min(free_list.Num(), n_spawn_requests)};
    if (entity_reuse_count > 0) {
        logger.log_verbose(TEXT("Reusing %d entities from free_list."), entity_reuse_count);
    }

    for (int32 i{0}; i < entity_reuse_count; ++i) {
        auto entity{free_list.Pop()};
        configure_active_bullet(
            entity_manager, entity, spawn_requests.transforms[i], spawn_requests.speeds[i]);
    }

    // Destroy remaining entities in free_list
    cmd_buffer.PushCommand<FMassDeferredDestroyCommand>([&](FMassEntityManager& entity_manager) {
        entity_manager.BatchDestroyEntities(free_list);
    });

    // Batch create new entities for unfulfilled spawn requests
    auto const remaining_spawn_requests{n_spawn_requests - entity_reuse_count};
    if (remaining_spawn_requests > 0) {
        logger.log_verbose(TEXT("Batch creating %d new entities."), remaining_spawn_requests);
        cmd_buffer.PushCommand<FMassDeferredCreateCommand>(
            [spawn_requests, entity_reuse_count, remaining_spawn_requests, this](
                FMassEntityManager& entity_manager) {
                TRACE_CPUPROFILER_EVENT_SCOPE(
                    TEXT("Sandbox::UMassBulletSubsystem::consume_lifecycle_requests::BatchCreate"))
                constexpr auto logger{NestedLogger<"consume_lifecycle_requests">()};

                new_entities.Reset(0);
                entity_manager.BatchCreateEntities(
                    bullet_archetype, shared_values, remaining_spawn_requests, new_entities);

                logger.log_verbose(TEXT("Configuring %d batch created entities."),
                                   new_entities.Num());

                auto const n{new_entities.Num()};
                for (int32 i{0}; i < n; ++i) {
                    auto const request_idx{entity_reuse_count + i};
                    configure_active_bullet(entity_manager,
                                            new_entities[i],
                                            spawn_requests.transforms[request_idx],
                                            spawn_requests.speeds[request_idx]);
                }
            });
    }

    entity_manager.FlushCommands();
    free_list.Reset(0);
}
