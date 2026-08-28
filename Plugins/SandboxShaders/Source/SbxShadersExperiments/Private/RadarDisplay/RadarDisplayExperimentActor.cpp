#include "SbxShadersExperiments/RadarDisplay/RadarDisplayExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRadarDisplayExperiment, Log, All);

namespace ml::shaders::radar_display {
constexpr TCHAR material_path[]{
    TEXT("/SandboxShaders/Experiments/RadarDisplay/M_RadarDisplay.M_RadarDisplay")};

auto contact_as_colour(FRadarContact const& contact) -> FLinearColor {
    return FLinearColor{static_cast<float>(contact.position.X),
                        static_cast<float>(contact.position.Y),
                        contact.size,
                        contact.intensity};
}
}

ARadarDisplayExperimentActor::ARadarDisplayExperimentActor() {
    PrimaryActorTick.bCanEverTick = false;

    display_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    SetRootComponent(display_mesh_);
    display_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    display_mesh_->SetCastShadow(false);
    display_mesh_->SetRelativeScale3D(FVector{6.5, 6.5, 1.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const plane_mesh{
        TEXT("/Engine/BasicShapes/Plane.Plane")};
    if (plane_mesh.Succeeded()) {
        display_mesh_->SetStaticMesh(plane_mesh.Object);
    } else {
        UE_LOG(LogRadarDisplayExperiment, Error, TEXT("Could not load the engine plane mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const display_material{
        ml::shaders::radar_display::material_path};
    if (display_material.Succeeded()) {
        display_material_ = display_material.Object;
        display_mesh_->SetMaterial(0, display_material_);
    }
}

void ARadarDisplayExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void ARadarDisplayExperimentActor::ensure_material() {
    if (!IsValid(display_material_)) {
        display_material_ =
            LoadObject<UMaterialInterface>(nullptr, ml::shaders::radar_display::material_path);
    }
    if (IsValid(display_material_) && !IsValid(material_instance_)) {
        material_instance_ = display_mesh_->CreateDynamicMaterialInstance(0, display_material_);
    }
}

void ARadarDisplayExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }

    material_instance_->SetVectorParameterValue(TEXT("BackgroundColour"),
                                                settings.background_colour);
    material_instance_->SetVectorParameterValue(TEXT("GridColour"), settings.grid_colour);
    material_instance_->SetVectorParameterValue(TEXT("SweepColour"), settings.sweep_colour);
    material_instance_->SetVectorParameterValue(TEXT("ContactColour"), settings.contact_colour);
    material_instance_->SetVectorParameterValue(
        TEXT("ContactA"), ml::shaders::radar_display::contact_as_colour(settings.contact_a));
    material_instance_->SetVectorParameterValue(
        TEXT("ContactB"), ml::shaders::radar_display::contact_as_colour(settings.contact_b));
    material_instance_->SetVectorParameterValue(
        TEXT("ContactC"), ml::shaders::radar_display::contact_as_colour(settings.contact_c));
    material_instance_->SetVectorParameterValue(
        TEXT("ContactD"), ml::shaders::radar_display::contact_as_colour(settings.contact_d));
    material_instance_->SetScalarParameterValue(TEXT("RingCount"),
                                                FMath::Clamp(settings.ring_count, 1.0f, 12.0f));
    material_instance_->SetScalarParameterValue(TEXT("SweepSpeed"),
                                                FMath::Max(settings.sweep_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("SweepWidth"),
                                                FMath::Clamp(settings.sweep_width, 0.005f, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("Interference"),
                                                FMath::Clamp(settings.interference, 0.0f, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveIntensity"),
                                                FMath::Max(settings.emissive_intensity, 0.0f));
}
