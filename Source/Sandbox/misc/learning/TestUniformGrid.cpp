#include "TestUniformGrid.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Logging/LogMacros.h"
#include "Materials/MaterialInstanceDynamic.h"

ATestUniformGrid::ATestUniformGrid()
    : volume_box{CreateDefaultSubobject<UBoxComponent>(TEXT("volume_box"))}
    , preview_mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("preview_mesh"))} {

    check(volume_box);
    check(preview_mesh);

    RootComponent = volume_box;

    preview_mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}
void ATestUniformGrid::BeginPlay() {
    Super::BeginPlay();
}
void ATestUniformGrid::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    volume_box->SetBoxExtent(box_extent);

    preview_mesh->SetVisibility(show_preview, true);
    // box_extent is half size
    // Assuming standard 100x100x100 cube
    preview_mesh->SetRelativeScale3D(box_extent * 2.0 / 100.0);

    create_material_instance();
}
void ATestUniformGrid::create_material_instance() {
    if (!preview_material_parent) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("preview_material_parent is nullptr."));
        return;
    }

    preview_material_instance = UMaterialInstanceDynamic::Create(preview_material_parent, this);
    preview_mesh->SetMaterial(0, preview_material_instance);

    preview_material_instance->SetVectorParameterValue(TEXT("colour"),
                                                       preview_material_settings.colour);
    preview_material_instance->SetScalarParameterValue(
        TEXT("opacity_edge_start"), preview_material_settings.opacity_edge_start);
}
