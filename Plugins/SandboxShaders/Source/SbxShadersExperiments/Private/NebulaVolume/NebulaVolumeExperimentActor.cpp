#include "SbxShadersExperiments/NebulaVolume/NebulaVolumeExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNebulaVolumeExperiment, Log, All);

namespace {
constexpr TCHAR nebula_volume_material_path[]{
    TEXT("/SandboxShaders/Experiments/NebulaVolume/M_NebulaVolume.M_NebulaVolume")};
}

ANebulaVolumeExperimentActor::ANebulaVolumeExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    volume_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NebulaVolume"));
    SetRootComponent(volume_mesh_);
    volume_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    volume_mesh_->SetCastShadow(false);
    volume_mesh_->SetRelativeScale3D(FVector{8.0, 6.0, 5.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const cube_mesh{
        TEXT("/Engine/BasicShapes/Cube.Cube")};
    if (cube_mesh.Succeeded()) {
        volume_mesh_->SetStaticMesh(cube_mesh.Object);
    } else {
        UE_LOG(LogNebulaVolumeExperiment, Error, TEXT("Could not load the engine cube mesh."));
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const volume_material{
        nebula_volume_material_path};
    if (volume_material.Succeeded()) {
        volume_material_ = volume_material.Object;
        volume_mesh_->SetMaterial(0, volume_material_);
    }
}

void ANebulaVolumeExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void ANebulaVolumeExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    apply_settings();
}

bool ANebulaVolumeExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void ANebulaVolumeExperimentActor::set_low_quality() {
    settings.step_count = 12;
    apply_settings();
}

void ANebulaVolumeExperimentActor::set_balanced_quality() {
    settings.step_count = 24;
    apply_settings();
}

void ANebulaVolumeExperimentActor::set_high_quality() {
    settings.step_count = 40;
    apply_settings();
}

void ANebulaVolumeExperimentActor::restart_animation() {
    settings.animation_paused = false;
    animation_time_ = 0.0f;
    apply_settings();
}

void ANebulaVolumeExperimentActor::ensure_material() {
    if (!IsValid(volume_material_)) {
        volume_material_ = LoadObject<UMaterialInterface>(nullptr, nebula_volume_material_path);
    }
    if (IsValid(volume_material_) && !IsValid(material_instance_)) {
        material_instance_ = volume_mesh_->CreateDynamicMaterialInstance(0, volume_material_);
    }
    if (!IsValid(material_instance_)) {
        UE_LOG(LogNebulaVolumeExperiment,
               Warning,
               TEXT("Nebula volume material is unavailable; regenerate showcase assets."));
    }
}

void ANebulaVolumeExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }

    material_instance_->SetVectorParameterValue(TEXT("ShadowColour"), settings.shadow_colour);
    material_instance_->SetVectorParameterValue(TEXT("EmissionColour"), settings.emission_colour);
    material_instance_->SetVectorParameterValue(
        TEXT("TextureOffset"),
        FLinearColor{static_cast<float>(settings.texture_offset.X),
                     static_cast<float>(settings.texture_offset.Y),
                     static_cast<float>(settings.texture_offset.Z),
                     0.0f});
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(TEXT("Density"),
                                                FMath::Clamp(settings.density, 0.0f, 4.0f));
    material_instance_->SetScalarParameterValue(TEXT("Extinction"),
                                                FMath::Clamp(settings.extinction, 0.0f, 8.0f));
    material_instance_->SetScalarParameterValue(TEXT("EmissiveStrength"),
                                                FMath::Max(settings.emissive_strength, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("NoiseScale"),
                                                FMath::Max(settings.noise_scale, 0.05f));
    material_instance_->SetScalarParameterValue(
        TEXT("DetailScale"), FMath::Clamp(settings.detail_scale, 1.0f, 8.0f));
    material_instance_->SetScalarParameterValue(
        TEXT("FlowStrength"), FMath::Clamp(settings.flow_strength, 0.0f, 2.0f));
    material_instance_->SetScalarParameterValue(TEXT("DriftSpeed"),
                                                FMath::Max(settings.drift_speed, 0.0f));
    material_instance_->SetScalarParameterValue(
        TEXT("StepCount"), static_cast<float>(FMath::Clamp(settings.step_count, 4, 48)));
}
