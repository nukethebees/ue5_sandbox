#include "SbxShadersExperiments/WarpField/WarpFieldExperimentActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace {
constexpr TCHAR warp_field_material_path[]{
    TEXT("/SandboxShaders/Experiments/WarpField/M_WarpField.M_WarpField")};
}

AWarpFieldExperimentActor::AWarpFieldExperimentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    field_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarpFieldMesh"));
    SetRootComponent(field_mesh_);
    field_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    field_mesh_->SetCastShadow(false);
    field_mesh_->SetRelativeScale3D(FVector{3.0});

    static ConstructorHelpers::FObjectFinder<UStaticMesh> const sphere_mesh{
        TEXT("/Engine/BasicShapes/Sphere.Sphere")};
    if (sphere_mesh.Succeeded()) {
        field_mesh_->SetStaticMesh(sphere_mesh.Object);
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> const field_material{
        warp_field_material_path};
    if (field_material.Succeeded()) {
        field_material_ = field_material.Object;
        field_mesh_->SetMaterial(0, field_material_);
    }
}

void AWarpFieldExperimentActor::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ensure_material();
    apply_settings();
}

void AWarpFieldExperimentActor::Tick(float const delta_seconds) {
    Super::Tick(delta_seconds);
    if (!settings.animation_paused) {
        animation_time_ += FMath::Max(delta_seconds, 0.0f);
    }
    ensure_material();
    apply_settings();
}

bool AWarpFieldExperimentActor::ShouldTickIfViewportsOnly() const {
    return true;
}

void AWarpFieldExperimentActor::restart_animation() {
    animation_time_ = 0.0f;
    apply_settings();
}

void AWarpFieldExperimentActor::ensure_material() {
    if (!IsValid(field_material_)) {
        field_material_ = LoadObject<UMaterialInterface>(nullptr, warp_field_material_path);
    }
    if (IsValid(field_material_) && !IsValid(material_instance_)) {
        material_instance_ = field_mesh_->CreateDynamicMaterialInstance(0, field_material_);
    }
}

void AWarpFieldExperimentActor::apply_settings() {
    if (!IsValid(material_instance_)) {
        return;
    }
    material_instance_->SetVectorParameterValue(TEXT("CoreColour"), settings.core_colour);
    material_instance_->SetVectorParameterValue(TEXT("EdgeColour"), settings.edge_colour);
    material_instance_->SetScalarParameterValue(TEXT("AnimationTime"), animation_time_);
    material_instance_->SetScalarParameterValue(
        TEXT("DistortionStrength"), FMath::Clamp(settings.distortion_strength, 0.0f, 1.0f));
    material_instance_->SetScalarParameterValue(TEXT("NoiseScale"),
                                                FMath::Max(settings.noise_scale, 0.1f));
    material_instance_->SetScalarParameterValue(TEXT("NoiseSpeed"),
                                                FMath::Max(settings.noise_speed, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("PulseFrequency"),
                                                FMath::Max(settings.pulse_frequency, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("EdgeIntensity"),
                                                FMath::Max(settings.edge_intensity, 0.0f));
    material_instance_->SetScalarParameterValue(TEXT("FieldOpacity"),
                                                FMath::Clamp(settings.opacity, 0.0f, 1.0f));
}
