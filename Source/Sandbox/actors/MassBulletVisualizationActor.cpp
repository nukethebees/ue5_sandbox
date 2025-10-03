#include "Sandbox/actors/MassBulletVisualizationActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/AssetManager.h"

#include "Sandbox/data_assets/BulletDataAsset.h"

#include "Sandbox/macros/null_checks.hpp"

AMassBulletVisualizationActor::AMassBulletVisualizationActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    ismc = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BulletISMC"));
    ismc->SetupAttachment(RootComponent);
}

void AMassBulletVisualizationActor::BeginPlay() {
    Super::BeginPlay();

    FPrimaryAssetId const primary_asset_id{TEXT("Bullet"), TEXT("Standard")};
    auto& asset_manager{UAssetManager::Get()};
    asset_manager.CallOrRegister_OnCompletedInitialScan(FSimpleDelegate::CreateUObject(
        this, &AMassBulletVisualizationActor::handle_assets_ready, primary_asset_id));
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
    if (!ismc || !bullet_data) {
        return std::nullopt;
    }

    if (free_indices.IsEmpty()) {
        grow_instances();
    }

    auto const index{free_indices.Pop()};
    ismc->UpdateInstanceTransform(index, transform, true, true);
    return index;
}

void AMassBulletVisualizationActor::update_instance(int32 instance_index,
                                                    FTransform const& transform) {
    RETURN_IF_NULLPTR(ismc);
    ismc->UpdateInstanceTransform(instance_index, transform, true, true);
}

void AMassBulletVisualizationActor::remove_instance(int32 instance_index) {
    RETURN_IF_NULLPTR(ismc);

    free_indices.Add(instance_index);
    ismc->UpdateInstanceTransform(instance_index, get_hidden_transform(), true, true);
}

void AMassBulletVisualizationActor::handle_preallocation() {
    if (preallocate_isms > 0) {
        log_display(TEXT("Preallocating %d ISM instances."), preallocate_isms);
        free_indices = create_instances(preallocate_isms);
    }
}
TArray<int32> AMassBulletVisualizationActor::create_instances(int32 n) {
    TArray<FTransform> transforms;
    transforms.Init(get_hidden_transform(), n);

    constexpr bool bShouldReturnIndices{true};
    constexpr bool bWorldSpace{true};
    constexpr bool bUpdateNavigation{false};

    return ismc->AddInstances(transforms, bShouldReturnIndices, bWorldSpace, bUpdateNavigation);
}
void AMassBulletVisualizationActor::add_instances(int32 n) {
    auto indexes{create_instances(n)};
    free_indices.Append(std::move(indexes));
}
void AMassBulletVisualizationActor::grow_instances() {
    RETURN_IF_NULLPTR(ismc);

    auto const n_ismcs{FMath::Max(ismc->GetNumInstances(), 1)};
    auto const target{
        (n_ismcs < 8) ? 8 : static_cast<int32>(static_cast<float>(n_ismcs) * growth_factor)};
    auto const to_add{target - n_ismcs};

    log_display(TEXT("Increasing ISM instances from %d to %d."), n_ismcs, target);

    add_instances(to_add);
}
FTransform const& AMassBulletVisualizationActor::get_hidden_transform() const {
    static FRotator const rotation{};
    static FVector const infinite_location{FLT_MAX, FLT_MAX, FLT_MAX};
    static auto const scale{FVector::OneVector};

    static FTransform const transform{rotation, infinite_location, scale};

    return transform;
}
