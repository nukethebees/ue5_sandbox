#include "Sandbox/actors/MassBulletVisualizationActor.h"

#include "Engine/AssetManager.h"
#include "MassSimulationSubsystem.h"

#include "Sandbox/data_assets/BulletDataAsset.h"

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

    (void)transform_queue.init(transform_queue_capacity);
}

void AMassBulletVisualizationActor::BeginPlay() {
    Super::BeginPlay();

    FPrimaryAssetId const primary_asset_id{TEXT("Bullet"), TEXT("Standard")};
    auto& asset_manager{UAssetManager::Get()};
    asset_manager.CallOrRegister_OnCompletedInitialScan(FSimpleDelegate::CreateUObject(
        this, &AMassBulletVisualizationActor::handle_assets_ready, primary_asset_id));

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

std::optional<int32> AMassBulletVisualizationActor::add_instance(FTransform const& transform) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::add_instance"))
    if (!ismc || !bullet_data) {
        return std::nullopt;
    }

    if (free_indices.IsEmpty()) {
        grow_instances();
    }

    auto const index{free_indices.Pop()};
    // update_transform(*ismc, index, transform);
    return index;
}

void AMassBulletVisualizationActor::update_instance(int32 instance_index,
                                                    FTransform const& transform) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::update_instance"))
    RETURN_IF_NULLPTR(ismc);
    update_transform(*ismc, instance_index, transform);
}

void AMassBulletVisualizationActor::remove_instance(int32 instance_index) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::remove_instance"))
    RETURN_IF_NULLPTR(ismc);

    free_indices.Add(instance_index);
    update_transform(*ismc, instance_index, get_hidden_transform());
}

void AMassBulletVisualizationActor::handle_preallocation() {
    TRACE_CPUPROFILER_EVENT_SCOPE(
        TEXT("Sandbox::AMassBulletVisualizationActor::handle_preallocation"))
    if (preallocate_isms > 0) {
        log_display(TEXT("Preallocating %d ISM instances."), preallocate_isms);
        free_indices = create_instances(preallocate_isms);
    }
}
TArray<int32> AMassBulletVisualizationActor::create_instances(int32 n) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::create_instances"))
    TArray<FTransform> transforms;
    transforms.Init(get_hidden_transform(), n);

    constexpr bool bShouldReturnIndices{true};
    constexpr bool bWorldSpace{true};
    constexpr bool bUpdateNavigation{false};

    return ismc->AddInstances(transforms, bShouldReturnIndices, bWorldSpace, bUpdateNavigation);
}
void AMassBulletVisualizationActor::add_instances(int32 n) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::add_instances"))
    auto indexes{create_instances(n)};
    free_indices.Append(std::move(indexes));
}
void AMassBulletVisualizationActor::grow_instances() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletVisualizationActor::grow_instances"))
    RETURN_IF_NULLPTR(ismc);

    constexpr int32 small_num{8};

    auto const n_ismcs{FMath::Max(ismc->GetNumInstances(), 1)};
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

    RETURN_IF_NULLPTR(ismc);

    auto queued_transforms{transform_queue.swap_and_consume()};
    auto n_transforms{queued_transforms.size()};
    if (n_transforms == 0) {
        return;
    }
    logger.log_verbose(TEXT("Batching %d transforms.\n"), n_transforms);

    constexpr int32 start_index{0};
    constexpr bool world_space{true};
    constexpr bool mark_dirty{true};
    constexpr bool teleport{true};

    TArrayView<FTransform const> transform_view(queued_transforms.data(), queued_transforms.size());

    ismc->BatchUpdateInstancesTransforms(
        start_index, transform_view, world_space, mark_dirty, teleport);
}
