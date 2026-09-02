#include "SbxShadersExperiments/NebulaBackdrop/NebulaBackdropExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNebulaBackdropExperiment, Log, All);

namespace {
constexpr TCHAR nebula_backdrop_material_path[]{
    TEXT("/SandboxShaders/Experiments/NebulaBackdrop/M_NebulaBackdrop.M_NebulaBackdrop")};
}

ANebulaBackdropExperimentActor::ANebulaBackdropExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    backdrop_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NebulaBackdrop"));
    SetRootComponent(backdrop_mesh_);
    backdrop_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    backdrop_mesh_->SetCastShadow(false);
    backdrop_mesh_->SetRelativeScale3D(FVector{12.0, 12.0, 1.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const plane_mesh{
        TEXT("/Engine/BasicShapes/Plane.Plane")};
    if (plane_mesh.Succeeded()) {
        backdrop_mesh_->SetStaticMesh(plane_mesh.Object);
    } else {
        UE_LOG(LogNebulaBackdropExperiment, Error, TEXT("Could not load the engine plane mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const backdrop_material{
        nebula_backdrop_material_path};
    if (backdrop_material.Succeeded()) {
        backdrop_material_ = backdrop_material.Object;
        backdrop_mesh_->SetMaterial(0, backdrop_material_);
    }
}

void ANebulaBackdropExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void ANebulaBackdropExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    apply_settings();
}

bool ANebulaBackdropExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void ANebulaBackdropExperimentActor::restart_animation() {
    settings.animation_paused = false;
    animation_time_ = 0.0f;
    apply_settings();
}

void ANebulaBackdropExperimentActor::ensure_material() {
    if (!IsValid(backdrop_material_)) {
        backdrop_material_ = LoadObject<UMaterialInterface>(nullptr, nebula_backdrop_material_path);
    }
    if (IsValid(backdrop_material_) && !IsValid(material_instance_)) {
        material_instance_ = backdrop_mesh_->CreateDynamicMaterialInstance(0, backdrop_material_);
    }
    if (!IsValid(material_instance_)) {
        UE_LOG(LogNebulaBackdropExperiment,
               Warning,
               TEXT("Nebula backdrop material is unavailable; regenerate showcase assets."));
    }
}

void ANebulaBackdropExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }

    material_instance_->SetVectorParameterValue(TEXT("ShadowColour"), settings.shadow_colour);
    material_instance_->SetVectorParameterValue(TEXT("EmissionColour"), settings.emission_colour);
    material_instance_->SetVectorParameterValue(
        TEXT("TextureOffset"),
        FLinearColor{static_cast<float>(settings.texture_offset.X),
                     static_cast<float>(settings.texture_offset.Y),
                     0.0f,
                     0.0f});
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(TEXT("Density"),
                                                FMath::Clamp(settings.density, 0.0f, 4.0f));
    material_instance_->SetScalarParameterValue(TEXT("Brightness"),
                                                FMath::Max(settings.brightness, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("TextureScale"),
                                                FMath::Max(settings.texture_scale, 0.05f));
    material_instance_->SetScalarParameterValue(
        TEXT("ParallaxStrength"), FMath::Clamp(settings.parallax_strength, 0.0f, 1.0f));
    material_instance_->SetScalarParameterValue(
        TEXT("EdgeSoftness"), FMath::Clamp(settings.edge_softness, 0.001f, 0.5f));
    material_instance_->SetScalarParameterValue(TEXT("DriftSpeed"),
                                                FMath::Max(settings.drift_speed, 0.0f));
}
