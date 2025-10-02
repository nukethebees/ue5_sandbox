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
}

std::optional<int32> AMassBulletVisualizationActor::add_instance(FTransform const& transform) {
    if (!ismc || !bullet_data) {
        return std::nullopt;
    }

    if (free_indices.Num() > 0) {
        auto const index{free_indices.Pop()};
        ismc->UpdateInstanceTransform(index, transform, true, true);
        return index;
    }

    return ismc->AddInstance(transform, true);
}

void AMassBulletVisualizationActor::update_instance(int32 instance_index,
                                                    FTransform const& transform) {
    RETURN_IF_NULLPTR(ismc);
    ismc->UpdateInstanceTransform(instance_index, transform, true, true);
}

void AMassBulletVisualizationActor::remove_instance(int32 instance_index) {
    RETURN_IF_NULLPTR(ismc);

    free_indices.Add(instance_index);
    ismc->UpdateInstanceTransform(
        instance_index, FTransform{FVector{0.0f, 0.0f, -10000.0f}}, true, true);
}
