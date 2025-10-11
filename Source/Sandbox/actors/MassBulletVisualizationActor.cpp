#include "Sandbox/actors/MassBulletVisualizationActor.h"

#include "Engine/AssetManager.h"
#include "MassSimulationSubsystem.h"

#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/data_assets/BulletDataAssetIds.h"

#include "Sandbox/macros/null_checks.hpp"

AMassBulletVisualizationActor::AMassBulletVisualizationActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    ismc = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BulletISMC"));
    ismc->SetupAttachment(RootComponent);

    // This is very important
    // If it's not disabled, updating any transform will force an update of all
    ismc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ismc->SetSimulatePhysics(false);
    ismc->SetEnableGravity(false);
    ismc->SetCanEverAffectNavigation(false);

    ismc->SetCastShadow(false);
    ismc->bCastDynamicShadow = false;
    ismc->bAffectDistanceFieldLighting = false;

    (void)transform_queue.logged_init(transform_queue_capacity, "MassBulletVisualizationActor");
}

void AMassBulletVisualizationActor::BeginPlay() {
    Super::BeginPlay();

    RETURN_IF_NULLPTR(ismc);

    ismc->SetCullDistances(cull_min_distance, cull_max_distance);

    auto& asset_manager{UAssetManager::Get()};
    asset_manager.CallOrRegister_OnCompletedInitialScan(FSimpleDelegate::CreateUObject(
        this, &AMassBulletVisualizationActor::handle_assets_ready, ml::bullet_ids::standard));

    register_phase_end_callback();
}

void AMassBulletVisualizationActor::handle_assets_ready(FPrimaryAssetId primary_asset_id) {
    auto& asset_manager{UAssetManager::Get()};
    constexpr auto logger{NestedLogger<"handle_assets_ready">()};

    TRY_INIT_PTR(loaded_obj, asset_manager.GetPrimaryAssetObject(primary_asset_id));
    TRY_INIT_PTR(loaded_data, Cast<UBulletDataAsset>(loaded_obj));
    bullet_data = loaded_data;
    TRY_INIT_PTR(mesh_component, bullet_data->mesh);

    RETURN_IF_NULLPTR(ismc);

    logger.log_display(TEXT("Setting up bullet mesh and material"));
    ismc->SetStaticMesh(mesh_component);
    ismc->SetMaterial(0, mesh_component->GetMaterial(0));

    handle_preallocation();
}

void AMassBulletVisualizationActor::handle_preallocation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::AMassBulletVisualizationActor::handle_preallocation"))
    if (preallocate_isms > 0) {
        log_display(TEXT("Preallocating %d ISM instances."), preallocate_isms);
        create_instances(preallocate_isms);
        current_instance_count = preallocate_isms;
    }
}
void AMassBulletVisualizationActor::create_instances(int32 n) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::create_instances"))
    TArray<FTransform> transforms;
    transforms.Init(get_hidden_transform(), n);

    constexpr bool bShouldReturnIndices{false};
    constexpr bool bWorldSpace{true};
    constexpr bool bUpdateNavigation{false};

    ismc->AddInstances(transforms, bShouldReturnIndices, bWorldSpace, bUpdateNavigation);
}
void AMassBulletVisualizationActor::add_instances(int32 n) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::add_instances"))
    create_instances(n);
    current_instance_count += n;
}
void AMassBulletVisualizationActor::grow_instances() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::grow_instances"))
    RETURN_IF_NULLPTR(ismc);

    constexpr int32 small_num{8};

    auto const n_ismcs{current_instance_count};
    auto const target{(n_ismcs < small_num)
                          ? small_num
                          : static_cast<int32>(static_cast<float>(n_ismcs) * growth_factor)};
    auto const to_add{target - n_ismcs};

    log_display(TEXT("Increasing ISM instances from %d to %d."), n_ismcs, target);

    add_instances(to_add);
}

void AMassBulletVisualizationActor::register_phase_end_callback() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::AMassBulletVisualizationActor::register_phase_end_callback"))
    constexpr auto logger{NestedLogger<"register_phase_end_callback">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(mass_simulation_subsystem, world->GetSubsystem<UMassSimulationSubsystem>());
    auto& phase_manager{mass_simulation_subsystem->GetMutablePhaseManager()};

    auto& phase_end_event{phase_manager.GetOnPhaseEnd(EMassProcessingPhase::FrameEnd)};
    phase_end_delegate_handle =
        phase_end_event.AddUObject(this, &AMassBulletVisualizationActor::on_phase_end);
    logger.log_display(TEXT("Registered phase end callback for FrameEnd phase"));
}

void AMassBulletVisualizationActor::on_phase_end(float delta_time) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::on_phase_end"))
    constexpr auto logger{NestedLogger<"on_phase_end">()};

    static FString const queue_name{TEXT("Transform")};

    auto const transform_result{transform_queue.swap_and_consume()};
    transform_result.log_results(queue_name);
    auto const n_to_hide{consume_killed_count()};

    RETURN_IF_NULLPTR(ismc);

    auto const n_to_transform{static_cast<int32>(transform_result.view.size())};
    auto const n_changes{n_to_hide + n_to_transform};

#if WITH_EDITOR
    num_flying = n_to_transform;
#endif

    if (n_changes == 0) {
        return;
    }

    if (n_to_transform > current_instance_count) {
        auto const to_add{n_to_transform - current_instance_count};
        logger.log_display(
            TEXT("Growing from %d to %d."), current_instance_count, n_to_transform, to_add);
        add_instances(to_add);
    }

    if (!logger.log_category.IsSuppressed(ELogVerbosity::VeryVerbose)) {
        logger.log_very_verbose(TEXT("Batching %d transforms.\n"), n_to_transform);
        for (auto const& xform : transform_result.view) {
            logger.log_very_verbose(TEXT("%s\n"), *xform.ToString());
        }
    }

    constexpr int32 start_index{0};
    constexpr bool world_space{true};
    constexpr bool teleport{true};
    bool const mark_dirty_on_active_update{n_to_transform && !n_to_hide};
    bool const mark_dirty_on_hidden_update{!n_to_transform && n_to_hide};
    bool const mark_dirty_at_end{n_to_transform && n_to_hide};

    if (n_to_transform) {
        TArrayView<FTransform const> transform_view(transform_result.view.data(), n_to_transform);
        ismc->BatchUpdateInstancesTransforms(
            start_index, transform_view, world_space, mark_dirty_on_active_update, teleport);
    }

    if (n_to_hide) {
        ismc->BatchUpdateInstancesTransform(n_to_transform,
                                            n_to_hide,
                                            get_hidden_transform(),
                                            world_space,
                                            mark_dirty_on_hidden_update,
                                            teleport);
    }

    if (mark_dirty_at_end) {
        ismc->MarkRenderInstancesDirty();
    }
}
