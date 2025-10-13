#include "Sandbox/actors/MassBulletVisualizationActor.h"

#include "Engine/AssetManager.h"
#include "MassSimulationSubsystem.h"

#include "Sandbox/actors/MassBulletSubsystemData.h"
#include "Sandbox/data_assets/BulletDataAsset.h"
#include "Sandbox/data_assets/BulletDataAssetIds.h"
#include "Sandbox/utilities/world.h"

#include "Sandbox/macros/null_checks.hpp"

AMassBulletVisualizationActor::AMassBulletVisualizationActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AMassBulletVisualizationActor::BeginPlay() {
    constexpr auto logger{NestedLogger<"BeginPlay">()};
    Super::BeginPlay();

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(data_actor, ml::get_first_actor<AMassBulletSubsystemData>(*world));

    logger.log_display(TEXT("Registering %d bullet types."), data_actor->bullet_types.Num());
    for (auto const& bullet_type : data_actor->bullet_types) {
        CONTINUE_IF_NULLPTR(bullet_type);
        add_new_mesh(*bullet_type);
    }

    handle_preallocation();
    register_phase_end_callback();
}

int32 AMassBulletVisualizationActor::add_new_mesh(UBulletDataAsset& bullet_data) {
    constexpr auto logger{NestedLogger<"add_new_mesh">()};
    auto const i{get_num_ismcs()};
    lookup.Add(bullet_data.GetPrimaryAssetId(), i);
    logger.log_display(TEXT("Adding mesh %d."), i);

    INIT_PTR_OR_RETURN_VALUE(ismc, NewObject<UInstancedStaticMeshComponent>(this), -1);

    ismc->RegisterComponent();
    ismc->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    ismcs.Add(ismc);

    // This is very important
    // If it's not disabled, updating any transform will force an update of all
    ismc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ismc->SetSimulatePhysics(false);
    ismc->SetEnableGravity(false);
    ismc->SetCanEverAffectNavigation(false);

    ismc->SetCastShadow(false);
    ismc->bCastDynamicShadow = false;
    ismc->bAffectDistanceFieldLighting = false;

    ismc->SetCullDistances(cull_min_distance, cull_max_distance);

    RETURN_VALUE_IF_NULLPTR(bullet_data.mesh, -1);
    RETURN_VALUE_IF_NULLPTR(bullet_data.material, -1);
    ismc->SetStaticMesh(bullet_data.mesh);
    ismc->SetMaterial(0, bullet_data.material);

    // Add the other elements to the arrays
    auto& queue{transform_queues.Emplace_GetRef()};
    current_instance_counts.Emplace(0);
    to_be_hidden.Emplace(0);

    // Configure the other elements
    (void)queue.logged_init(transform_queue_capacity, "MassBulletVisualizationActor");

    return i;
}

void AMassBulletVisualizationActor::handle_preallocation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::AMassBulletVisualizationActor::handle_preallocation"))
    if (preallocate_isms > 0) {
        log_display(TEXT("Preallocating %d ISM instances."), preallocate_isms);
        add_instances(preallocate_isms);
    }
}
void AMassBulletVisualizationActor::add_instances(int32 n) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::add_instances"))
    constexpr auto logger{NestedLogger<"add_instances">()};

    TArray<FTransform> transforms;
    transforms.Init(get_hidden_transform(), n);

    constexpr bool bShouldReturnIndices{false};
    constexpr bool bWorldSpace{true};
    constexpr bool bUpdateNavigation{false};

    auto const n_components{get_num_ismcs()};
    for (int32 i{0}; i < n_components; ++i) {
        TRY_INIT_PTR(ismc, ismcs[i]);
        ismc->AddInstances(transforms, bShouldReturnIndices, bWorldSpace, bUpdateNavigation);
        current_instance_counts[i] += n;
    }

    logger.log_display(TEXT("Added %d instances."), n);
}
void AMassBulletVisualizationActor::grow_instances() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::grow_instances"))
    constexpr auto logger{NestedLogger<"grow_instances">()};

    constexpr int32 small_num{8};
    auto const n{get_num_ismcs()};

    for (int32 i{0}; i < n; ++i) {
        TRY_INIT_PTR(ismc, ismcs[i]);

        auto const n_ismcs{current_instance_counts[i]};
        auto const target{(n_ismcs < small_num)
                              ? small_num
                              : static_cast<int32>(static_cast<float>(n_ismcs) * growth_factor)};
        auto const to_add{target - n_ismcs};

        logger.log_display(TEXT("Increasing ISM instances from %d to %d."), n_ismcs, target);

        add_instances(to_add);
    }
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

    auto const n{get_num_ismcs()};
    for (int32 i{0}; i < n; ++i) {
        TRY_INIT_PTR(ismc, ismcs[i]);
        auto const current_instance_count{current_instance_counts[i]};

        auto const transform_result{transform_queues[i].swap_and_consume()};
        transform_result.log_results(queue_name);
        auto const n_to_hide{consume_killed_count(i)};

        auto const n_to_transform{static_cast<int32>(transform_result.view.size())};
        auto const n_changes{n_to_hide + n_to_transform};

        if (n_changes == 0) {
            continue;
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
            TArrayView<FTransform const> transform_view(transform_result.view.data(),
                                                        n_to_transform);
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
}
