#include "SbxShadersExperiments/VertexRipple/VertexRippleExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVertexRippleExperiment, Log, All);

namespace {
constexpr TCHAR vertex_ripple_mesh_path[]{
    TEXT("/SandboxShaders/Experiments/VertexRipple/SM_VertexRippleGrid.SM_VertexRippleGrid")};
constexpr TCHAR vertex_ripple_material_path[]{
    TEXT("/SandboxShaders/Experiments/VertexRipple/M_VertexRipple.M_VertexRipple")};
}

AVertexRippleExperimentActor::AVertexRippleExperimentActor() {
    PrimaryActorTick.bCanEverTick = false;

    surface_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SurfaceMesh"));
    SetRootComponent(surface_mesh_);
    surface_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    surface_mesh_->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const surface_mesh{
        vertex_ripple_mesh_path};
    if (surface_mesh.Succeeded()) {
        surface_mesh_->SetStaticMesh(surface_mesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const surface_material{
        vertex_ripple_material_path};
    if (surface_material.Succeeded()) {
        surface_material_ = surface_material.Object;
        surface_mesh_->SetMaterial(0, surface_material_);
    }
}

void AVertexRippleExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void AVertexRippleExperimentActor::ensure_material() {
    if (!IsValid(surface_mesh_->GetStaticMesh())) {
        surface_mesh_->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, vertex_ripple_mesh_path));
    }
    if (!IsValid(surface_material_)) {
        surface_material_ = LoadObject<UMaterialInterface>(nullptr, vertex_ripple_material_path);
    }
    if (IsValid(surface_material_) && !IsValid(material_instance_)) {
        material_instance_ = surface_mesh_->CreateDynamicMaterialInstance(0, surface_material_);
    }
}

void AVertexRippleExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }

    material_instance_->SetVectorParameterValue(
        TEXT("RippleOrigin"),
        FLinearColor{static_cast<float>(settings.origin_uv.X),
                     static_cast<float>(settings.origin_uv.Y),
                     0.0f,
                     0.0f});
    material_instance_->SetVectorParameterValue(TEXT("BaseColour"), settings.base_colour);
    material_instance_->SetVectorParameterValue(TEXT("CrestColour"), settings.crest_colour);
    material_instance_->SetScalarParameterValue(TEXT("Amplitude"),
                                                FMath::Max(settings.amplitude, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("WaveSpeed"),
                                                FMath::Max(settings.wave_speed, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("Wavelength"),
                                                FMath::Max(settings.wavelength, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("WaveWidth"),
                                                FMath::Clamp(settings.wave_width, 0.005f, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("Falloff"),
                                                FMath::Max(settings.falloff, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveIntensity"),
                                                FMath::Max(settings.emissive_intensity, 0.0f));
}
