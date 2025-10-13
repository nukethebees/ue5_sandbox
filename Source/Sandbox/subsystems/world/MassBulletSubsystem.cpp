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
        this, &UMassBulletSubsystem::on_archetypes_ready);

    logger.log_display(TEXT("Initialised."));
}
void UMassBulletSubsystem::Deinitialize() {
    FCoreDelegates::OnEndFrame.RemoveAll(this);
    Super::Deinitialize();
}

void UMassBulletSubsystem::on_archetypes_ready() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::on_archetypes_ready"))
    constexpr auto logger{NestedLogger<"on_archetypes_ready">()};
    RETURN_IF_FALSE(initialise_asset_data());
    FCoreDelegates::OnEndFrame.AddUObject(this, &UMassBulletSubsystem::on_end_frame);
    logger.log_display(TEXT("Ready."));
}
bool UMassBulletSubsystem::initialise_asset_data() {
    constexpr auto logger{NestedLogger<"initialise_asset_data">()};

    INIT_PTR_OR_RETURN_VALUE(world, GetWorld(), false);
    INIT_PTR_OR_RETURN_VALUE(
        archetype_subsystem, world->GetSubsystem<UMassArchetypeSubsystem>(), false);
    INIT_PTR_OR_RETURN_VALUE(data_actor, get_data_actor(), false);
    RETURN_VALUE_IF_TRUE(data_actor->bullet_types.IsEmpty(), false);

    for (auto const& bullet_data : data_actor->bullet_types) {
        CONTINUE_IF_NULLPTR(bullet_data);

        FPrimaryAssetId const asset_id{bullet_data->GetPrimaryAssetId()};
        INIT_OPTIONAL_OR_RETURN_VALUE(
            entity_def, archetype_subsystem->get_definition(asset_id), false);

        bullet_definitions.Add(asset_id, *entity_def);
        logger.log_display(TEXT("Loaded bullet type: %s"), *asset_id.ToString());
    }

    RETURN_VALUE_IF_TRUE(bullet_definitions.IsEmpty(), false);

    return true;
}
void UMassBulletSubsystem::configure_active_bullet(FMassEntityManager& entity_manager,
                                                   FMassEntityHandle entity,
                                                   FTransform const& transform,
                                                   float bullet_speed) {
    constexpr auto logger{NestedLogger<"configure_active_bullet">()};

    auto get_frag{
        [&]<typename T>() -> auto& { return entity_manager.GetFragmentDataChecked<T>(entity); }};
#define GET_FRAG(T) get_frag.operator()<T>()

    auto& transform_frag{GET_FRAG(FMassBulletTransformFragment)};
    transform_frag.transform = transform;

    auto& velocity_frag{GET_FRAG(FMassBulletVelocityFragment)};
    auto const velocity{transform.Rotator().Vector().GetSafeNormal() * bullet_speed};
    velocity_frag.velocity = velocity;

    auto& last_pos_frag{GET_FRAG(FMassBulletLastPositionFragment)};
    last_pos_frag.last_position = transform.GetLocation();

    auto& hit_info_frag{GET_FRAG(FMassBulletHitInfoFragment)};
    hit_info_frag = {FVector::ZeroVector, FVector::ZeroVector, nullptr};

    auto& state_frag{GET_FRAG(FMassBulletStateFragment)};
    state_frag.hit_occurred = false;
#undef GET_FRAG
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
    spawns.log_results(TEXT("UMassBulletSubsystem::spawns"));

    auto destroys{destroy_queue.swap_and_consume()};
    destroys.log_results(TEXT("UMassBulletSubsystem::destroys"));

    consume_lifecycle_requests(spawns.view, destroys.view);
}
void UMassBulletSubsystem::consume_lifecycle_requests(
    FBulletSpawnRequestView const& spawn_requests,
    FBulletDestroyRequestView const& destroy_requests) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UMassBulletSubsystem::consume_lifecycle_requests"))
    constexpr auto logger{NestedLogger<"consume_lifecycle_requests">()};

    auto const n_destroy_requests{static_cast<int32>(destroy_requests.entities.size())};
    auto const n_spawn_requests{static_cast<int32>(spawn_requests.transforms.size())};
    auto const n_requests{n_spawn_requests + n_destroy_requests};

    if (n_requests == 0) {
        return;
    }

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_subsystem, world->GetSubsystem<UMassEntitySubsystem>());
    auto& entity_manager{mass_subsystem->GetMutableEntityManager()};
    auto& cmd_buffer{entity_manager.Defer()};

    struct FBulletTypeRequests {
        TArray<int32> spawn_indices;
        TArray<int32> destroy_indices;
    };
    TMap<FPrimaryAssetId, FBulletTypeRequests> requests_by_type;

    // Group spawn requests by bullet type
    for (int32 i{0}; i < n_spawn_requests; ++i) {
        FPrimaryAssetId const& bullet_type{spawn_requests.bullet_types[i]};
        if (!bullet_definitions.Contains(bullet_type)) {
            logger.log_warning(TEXT("Unregistered bullet type: %s"), *bullet_type.ToString());
            continue;
        }
        requests_by_type.FindOrAdd(bullet_type).spawn_indices.Add(i);
    }

    // Group destroy requests by bullet type
    for (int32 i{0}; i < n_destroy_requests; ++i) {
        FPrimaryAssetId const& bullet_type{destroy_requests.bullet_types[i]};
        if (!bullet_definitions.Contains(bullet_type)) {
            logger.log_warning(TEXT("Unregistered bullet type: %s"), *bullet_type.ToString());
            continue;
        }
        requests_by_type.FindOrAdd(bullet_type).destroy_indices.Add(i);
    }

    // Process each bullet type
    for (auto const& [bullet_type, type_requests] : requests_by_type) {
        FEntityDefinition const* bullet_definition{bullet_definitions.Find(bullet_type)};
        if (!bullet_definition) {
            continue;
        }

        auto const& spawn_indices{type_requests.spawn_indices};
        auto const& destroy_indices{type_requests.destroy_indices};
        auto const n_spawns{spawn_indices.Num()};
        auto const n_destroys{destroy_indices.Num()};

        // Add all destroys for this type to free_list
        free_list.Reset(0);
        for (int32 idx : destroy_indices) {
            free_list.Add(destroy_requests.entities[idx]);
        }

        // Reuse entities from free_list for spawns
        auto const n_reused{FMath::Min(free_list.Num(), n_spawns)};
        for (int32 i{0}; i < n_reused; ++i) {
            auto const spawn_idx{spawn_indices[i]};
            configure_active_bullet(entity_manager,
                                    free_list.Pop(),
                                    spawn_requests.transforms[spawn_idx],
                                    spawn_requests.speeds[spawn_idx]);
        }

        // Destroy remaining entities in free_list
        auto const n_destroyed{free_list.Num()};
        if (n_destroyed > 0) {
            TArray<FMassEntityHandle> entities_to_destroy{free_list};
            cmd_buffer.PushCommand<FMassDeferredDestroyCommand>(
                [entities_to_destroy](FMassEntityManager& entity_manager) {
                    entity_manager.BatchDestroyEntities(entities_to_destroy);
                });
        }

        // Batch create new entities for unfulfilled spawn requests
        auto const remaining_spawns{n_spawns - n_reused};
        if (remaining_spawns > 0) {
            cmd_buffer.PushCommand<FMassDeferredCreateCommand>(
                [spawn_requests,
                 spawn_indices,
                 n_reused,
                 remaining_spawns,
                 bullet_definition,
                 this](FMassEntityManager& entity_manager) {
                    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT(
                        "Sandbox::UMassBulletSubsystem::consume_lifecycle_requests::BatchCreate"))

                    new_entities.Reset(0);
                    entity_manager.BatchCreateEntities(bullet_definition->archetype,
                                                       bullet_definition->shared_fragments,
                                                       remaining_spawns,
                                                       new_entities);

                    for (int32 i{0}; i < remaining_spawns; ++i) {
                        auto const spawn_idx{spawn_indices[n_reused + i]};
                        configure_active_bullet(entity_manager,
                                                new_entities[i],
                                                spawn_requests.transforms[spawn_idx],
                                                spawn_requests.speeds[spawn_idx]);
                    }
                });
        }

        logger.log_verbose(TEXT("Type %s: %d created (%d reused, %d new), %d destroyed."),
                           *bullet_type.ToString(),
                           n_spawns,
                           n_reused,
                           remaining_spawns,
                           n_destroyed);
    }

    entity_manager.FlushCommands();
    free_list.Reset(0);
}
